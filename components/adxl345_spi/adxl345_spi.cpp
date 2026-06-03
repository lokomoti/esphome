#include "adxl345_spi.h"
#include "esphome/core/log.h"
#include <cmath>
#include <vector>

namespace esphome {
namespace adxl345_spi {

static const char *const TAG = "adxl345_spi";

static const uint8_t ADXL345_REG_DEVID = 0x00;
static const uint8_t ADXL345_REG_BW_RATE = 0x2C;
static const uint8_t ADXL345_REG_POWER_CTL = 0x2D;
static const uint8_t ADXL345_REG_DATA_FORMAT = 0x31;
static const uint8_t ADXL345_REG_DATAX0 = 0x32;
static const uint8_t ADXL345_REG_FIFO_CTL = 0x38;
static const uint8_t ADXL345_REG_FIFO_STATUS = 0x39;

static const float ADXL345_MG_PER_LSB = 3.9f;

void ADXL345SPIComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADXL345 SPI component...");
  this->spi_setup();

  delay(100);

  uint8_t devid_before = this->read_register_(ADXL345_REG_DEVID);
  ESP_LOGI(TAG, "ADXL345 DEVID before init: 0x%02X", devid_before);

  this->write_register_(ADXL345_REG_POWER_CTL, 0x00);
  this->write_register_(ADXL345_REG_BW_RATE, static_cast<uint8_t>(this->data_rate_));

  uint8_t data_format = static_cast<uint8_t>(this->range_);
  if (this->full_resolution_) {
    data_format |= 0x08;
  }
  this->write_register_(ADXL345_REG_DATA_FORMAT, data_format);

  // FIFO stream mode, watermark = 0
  this->write_register_(ADXL345_REG_FIFO_CTL, 0x80);

  // Measurement mode
  this->write_register_(ADXL345_REG_POWER_CTL, 0x08);

  delay(10);

  uint8_t devid = this->read_register_(ADXL345_REG_DEVID);
  uint8_t power_ctl = this->read_register_(ADXL345_REG_POWER_CTL);
  uint8_t configured_data_format = this->read_register_(ADXL345_REG_DATA_FORMAT);
  uint8_t bw_rate = this->read_register_(ADXL345_REG_BW_RATE);
  uint8_t fifo_ctl = this->read_register_(ADXL345_REG_FIFO_CTL);

  ESP_LOGI(TAG, "ADXL345 DEVID: 0x%02X", devid);
  ESP_LOGI(TAG, "ADXL345 POWER_CTL: 0x%02X", power_ctl);
  ESP_LOGI(TAG, "ADXL345 DATA_FORMAT: 0x%02X", configured_data_format);
  ESP_LOGI(TAG, "ADXL345 BW_RATE: 0x%02X", bw_rate);
  ESP_LOGI(TAG, "ADXL345 FIFO_CTL: 0x%02X", fifo_ctl);

  this->reset_window_();
}

void ADXL345SPIComponent::loop() {
  uint8_t sample_count = this->read_fifo_sample_count_();
  if (sample_count == 0) {
    return;
  }

  if (sample_count > 32) {
    sample_count = 32;
  }

  if (sample_count >= 31) {
    this->fifo_overruns_++;
  }

  for (uint8_t i = 0; i < sample_count; i++) {
    uint8_t data[6];
    this->read_multiple_(ADXL345_REG_DATAX0, data, 6);

    int16_t raw_x = static_cast<int16_t>((data[1] << 8) | data[0]);
    int16_t raw_y = static_cast<int16_t>((data[3] << 8) | data[2]);
    int16_t raw_z = static_cast<int16_t>((data[5] << 8) | data[4]);

    float x_g = this->raw_to_g_(raw_x);
    float y_g = this->raw_to_g_(raw_y);
    float z_g = this->raw_to_g_(raw_z);

    this->process_sample_(x_g, y_g, z_g);
  }
}

void ADXL345SPIComponent::process_sample_(float x_g, float y_g, float z_g) {
  if (!this->baseline_initialized_) {
    this->baseline_x_ = x_g;
    this->baseline_y_ = y_g;
    this->baseline_z_ = z_g;
    this->baseline_initialized_ = true;
  } else {
    this->baseline_x_ = (1.0f - this->baseline_alpha_) * this->baseline_x_ + this->baseline_alpha_ * x_g;
    this->baseline_y_ = (1.0f - this->baseline_alpha_) * this->baseline_y_ + this->baseline_alpha_ * y_g;
    this->baseline_z_ = (1.0f - this->baseline_alpha_) * this->baseline_z_ + this->baseline_alpha_ * z_g;
  }

  float dx = x_g - this->baseline_x_;
  float dy = y_g - this->baseline_y_;
  float dz = z_g - this->baseline_z_;

  float magnitude = sqrtf(x_g * x_g + y_g * y_g + z_g * z_g);
  float dynamic_magnitude = sqrtf(dx * dx + dy * dy + dz * dz);

  this->last_x_g_ = x_g;
  this->last_y_g_ = y_g;
  this->last_z_g_ = z_g;
  this->last_magnitude_ = magnitude;
  this->last_dynamic_magnitude_ = dynamic_magnitude;

  this->window_count_++;
  this->window_sum_dynamic_ += dynamic_magnitude;
  this->window_sum_dynamic_sq_ += dynamic_magnitude * dynamic_magnitude;
  if (dynamic_magnitude > this->window_peak_dynamic_) {
    this->window_peak_dynamic_ = dynamic_magnitude;
  }
}

void ADXL345SPIComponent::update() {
  float mean_dynamic = 0.0f;
  float rms = 0.0f;

  if (this->window_count_ > 0) {
    mean_dynamic = this->window_sum_dynamic_ / this->window_count_;
    rms = sqrtf(this->window_sum_dynamic_sq_ / this->window_count_);
  }

  ESP_LOGD(TAG,
           "X=%.3f g Y=%.3f g Z=%.3f g | Mag=%.3f g | DynMean=%.3f g | RMS=%.3f g | Peak=%.3f g | Samples=%u | FIFO overruns=%u",
           this->last_x_g_, this->last_y_g_, this->last_z_g_, this->last_magnitude_,
           mean_dynamic, rms, this->window_peak_dynamic_, this->window_count_, this->fifo_overruns_);

  if (this->x_sensor_ != nullptr)
    this->x_sensor_->publish_state(this->last_x_g_);
  if (this->y_sensor_ != nullptr)
    this->y_sensor_->publish_state(this->last_y_g_);
  if (this->z_sensor_ != nullptr)
    this->z_sensor_->publish_state(this->last_z_g_);
  if (this->magnitude_sensor_ != nullptr)
    this->magnitude_sensor_->publish_state(this->last_magnitude_);
  if (this->dynamic_magnitude_sensor_ != nullptr)
    this->dynamic_magnitude_sensor_->publish_state(mean_dynamic);
  if (this->rms_sensor_ != nullptr)
    this->rms_sensor_->publish_state(rms);

  this->reset_window_();
}

void ADXL345SPIComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "ADXL345 SPI");
  ESP_LOGCONFIG(TAG, "  Data Rate: 0x%02X", static_cast<uint8_t>(this->data_rate_));
  ESP_LOGCONFIG(TAG, "  Range: 0x%02X", static_cast<uint8_t>(this->range_));
  ESP_LOGCONFIG(TAG, "  Full Resolution: %s", this->full_resolution_ ? "true" : "false");

  LOG_SENSOR("  ", "X", this->x_sensor_);
  LOG_SENSOR("  ", "Y", this->y_sensor_);
  LOG_SENSOR("  ", "Z", this->z_sensor_);
  LOG_SENSOR("  ", "Magnitude", this->magnitude_sensor_);
  LOG_SENSOR("  ", "Dynamic Magnitude", this->dynamic_magnitude_sensor_);
  LOG_SENSOR("  ", "RMS", this->rms_sensor_);
}

uint8_t ADXL345SPIComponent::read_register_(uint8_t reg) {
  this->enable();
  this->transfer_byte(reg | 0x80);
  uint8_t value = this->transfer_byte(0x00);
  this->disable();
  return value;
}

void ADXL345SPIComponent::write_register_(uint8_t reg, uint8_t value) {
  this->enable();
  this->transfer_byte(reg & 0x3F);
  this->transfer_byte(value);
  this->disable();
}

void ADXL345SPIComponent::read_multiple_(uint8_t reg, uint8_t *data, size_t len) {
  this->enable();
  this->transfer_byte(reg | 0xC0);
  for (size_t i = 0; i < len; i++) {
    data[i] = this->transfer_byte(0x00);
  }
  this->disable();
}

uint8_t ADXL345SPIComponent::read_fifo_sample_count_() {
  uint8_t fifo_status = this->read_register_(ADXL345_REG_FIFO_STATUS);
  return fifo_status & 0x3F;
}

float ADXL345SPIComponent::raw_to_g_(int16_t raw) const {
  if (this->full_resolution_) {
    return (raw * ADXL345_MG_PER_LSB) / 1000.0f;
  }

  float range_factor = 1.0f;
  switch (this->range_) {
    case RANGE_2G:
      range_factor = 1.0f;
      break;
    case RANGE_4G:
      range_factor = 2.0f;
      break;
    case RANGE_8G:
      range_factor = 4.0f;
      break;
    case RANGE_16G:
      range_factor = 8.0f;
      break;
  }

  return (raw * ADXL345_MG_PER_LSB * range_factor) / 1000.0f;
}

float ADXL345SPIComponent::get_output_data_rate_hz_() const {
  switch (this->data_rate_) {
    case DATA_RATE_0_10: return 0.10f;
    case DATA_RATE_0_20: return 0.20f;
    case DATA_RATE_0_39: return 0.39f;
    case DATA_RATE_0_78: return 0.78f;
    case DATA_RATE_1_56: return 1.56f;
    case DATA_RATE_3_13: return 3.13f;
    case DATA_RATE_6_25: return 6.25f;
    case DATA_RATE_12_5: return 12.5f;
    case DATA_RATE_25: return 25.0f;
    case DATA_RATE_50: return 50.0f;
    case DATA_RATE_100: return 100.0f;
    case DATA_RATE_200: return 200.0f;
    case DATA_RATE_400: return 400.0f;
    case DATA_RATE_800: return 800.0f;
    case DATA_RATE_1600: return 1600.0f;
    case DATA_RATE_3200: return 3200.0f;
  }
  return 100.0f;
}

void ADXL345SPIComponent::reset_window_() {
  this->window_count_ = 0;
  this->window_sum_dynamic_ = 0.0f;
  this->window_sum_dynamic_sq_ = 0.0f;
  this->window_peak_dynamic_ = 0.0f;
}

}  // namespace adxl345_spi
}  // namespace esphome