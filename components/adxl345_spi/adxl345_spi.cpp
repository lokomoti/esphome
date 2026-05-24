#include "adxl345_spi.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome
{
    namespace adxl345_spi
    {

        static const char *const TAG = "adxl345_spi";

        static const uint8_t ADXL345_REG_DEVID = 0x00;
        static const uint8_t ADXL345_REG_BW_RATE = 0x2C;
        static const uint8_t ADXL345_REG_POWER_CTL = 0x2D;
        static const uint8_t ADXL345_REG_DATA_FORMAT = 0x31;
        static const uint8_t ADXL345_REG_DATAX0 = 0x32;

        static const float ADXL345_MG_PER_LSB = 3.9f;

        void ADXL345SPIComponent::setup()
        {
            ESP_LOGCONFIG(TAG, "Setting up ADXL345 SPI component...");
            this->spi_setup();

            delay(100);

            uint8_t devid_before = this->read_register_(ADXL345_REG_DEVID);
            ESP_LOGI(TAG, "ADXL345 DEVID before init: 0x%02X", devid_before);

            this->write_register_(ADXL345_REG_POWER_CTL, 0x00);
            this->write_register_(ADXL345_REG_BW_RATE, static_cast<uint8_t>(this->data_rate_));

            uint8_t data_format = static_cast<uint8_t>(this->range_);
            if (this->full_resolution_)
            {
                data_format |= 0x08;
            }
            this->write_register_(ADXL345_REG_DATA_FORMAT, data_format);
            this->write_register_(ADXL345_REG_POWER_CTL, 0x08);

            delay(10);

            uint8_t devid = this->read_register_(ADXL345_REG_DEVID);
            uint8_t power_ctl = this->read_register_(ADXL345_REG_POWER_CTL);
            uint8_t configured_data_format = this->read_register_(ADXL345_REG_DATA_FORMAT);
            uint8_t bw_rate = this->read_register_(ADXL345_REG_BW_RATE);

            ESP_LOGI(TAG, "ADXL345 DEVID: 0x%02X", devid);
            ESP_LOGI(TAG, "ADXL345 POWER_CTL: 0x%02X", power_ctl);
            ESP_LOGI(TAG, "ADXL345 DATA_FORMAT: 0x%02X", configured_data_format);
            ESP_LOGI(TAG, "ADXL345 BW_RATE: 0x%02X", bw_rate);
        }

        void ADXL345SPIComponent::update()
        {
            int16_t raw_x = 0, raw_y = 0, raw_z = 0;
            this->read_raw_xyz_(raw_x, raw_y, raw_z);

            float x_g = this->raw_to_g_(raw_x);
            float y_g = this->raw_to_g_(raw_y);
            float z_g = this->raw_to_g_(raw_z);
            float magnitude = sqrtf(x_g * x_g + y_g * y_g + z_g * z_g);

            float rms_accumulator = 0.0f;
            float last_dynamic_magnitude = 0.0f;

            for (uint16_t i = 0; i < this->samples_per_update_; i++)
            {
                int16_t sx = 0, sy = 0, sz = 0;
                this->read_raw_xyz_(sx, sy, sz);

                float sx_g = this->raw_to_g_(sx);
                float sy_g = this->raw_to_g_(sy);
                float sz_g = this->raw_to_g_(sz);

                float sample_magnitude = sqrtf(sx_g * sx_g + sy_g * sy_g + sz_g * sz_g);
                float dynamic = sample_magnitude - 1.0f;
                float dynamic_abs = fabsf(dynamic);

                last_dynamic_magnitude = dynamic_abs;
                rms_accumulator += dynamic * dynamic;
            }

            float rms = sqrtf(rms_accumulator / this->samples_per_update_);

            ESP_LOGD(TAG, "Raw X=%d Y=%d Z=%d", raw_x, raw_y, raw_z);
            ESP_LOGD(TAG, "X=%.3f g Y=%.3f g Z=%.3f g | Mag=%.3f g | Dyn=%.3f g | RMS=%.3f g",
                     x_g, y_g, z_g, magnitude, last_dynamic_magnitude, rms);

            if (this->x_sensor_ != nullptr)
                this->x_sensor_->publish_state(x_g);
            if (this->y_sensor_ != nullptr)
                this->y_sensor_->publish_state(y_g);
            if (this->z_sensor_ != nullptr)
                this->z_sensor_->publish_state(z_g);
            if (this->magnitude_sensor_ != nullptr)
                this->magnitude_sensor_->publish_state(magnitude);
            if (this->dynamic_magnitude_sensor_ != nullptr)
                this->dynamic_magnitude_sensor_->publish_state(last_dynamic_magnitude);
            if (this->rms_sensor_ != nullptr)
                this->rms_sensor_->publish_state(rms);
        }

        void ADXL345SPIComponent::dump_config()
        {
            ESP_LOGCONFIG(TAG, "ADXL345 SPI");
            ESP_LOGCONFIG(TAG, "  Data Rate: 0x%02X", static_cast<uint8_t>(this->data_rate_));
            ESP_LOGCONFIG(TAG, "  Range: 0x%02X", static_cast<uint8_t>(this->range_));
            ESP_LOGCONFIG(TAG, "  Full Resolution: %s", this->full_resolution_ ? "true" : "false");
            ESP_LOGCONFIG(TAG, "  Samples per update: %u", this->samples_per_update_);

            LOG_SENSOR("  ", "X", this->x_sensor_);
            LOG_SENSOR("  ", "Y", this->y_sensor_);
            LOG_SENSOR("  ", "Z", this->z_sensor_);
            LOG_SENSOR("  ", "Magnitude", this->magnitude_sensor_);
            LOG_SENSOR("  ", "Dynamic Magnitude", this->dynamic_magnitude_sensor_);
            LOG_SENSOR("  ", "RMS", this->rms_sensor_);
        }

        uint8_t ADXL345SPIComponent::read_register_(uint8_t reg)
        {
            this->enable();
            this->transfer_byte(reg | 0x80);
            uint8_t value = this->transfer_byte(0x00);
            this->disable();
            return value;
        }

        void ADXL345SPIComponent::write_register_(uint8_t reg, uint8_t value)
        {
            this->enable();
            this->transfer_byte(reg & 0x3F);
            this->transfer_byte(value);
            this->disable();
        }

        void ADXL345SPIComponent::read_multiple_(uint8_t reg, uint8_t *data, size_t len)
        {
            this->enable();
            this->transfer_byte(reg | 0xC0);
            for (size_t i = 0; i < len; i++)
            {
                data[i] = this->transfer_byte(0x00);
            }
            this->disable();
        }

        void ADXL345SPIComponent::read_raw_xyz_(int16_t &x, int16_t &y, int16_t &z)
        {
            uint8_t data[6];
            this->read_multiple_(ADXL345_REG_DATAX0, data, 6);

            x = static_cast<int16_t>((data[1] << 8) | data[0]);
            y = static_cast<int16_t>((data[3] << 8) | data[2]);
            z = static_cast<int16_t>((data[5] << 8) | data[4]);
        }

        float ADXL345SPIComponent::raw_to_g_(int16_t raw) const
        {
            if (this->full_resolution_)
            {
                return (raw * ADXL345_MG_PER_LSB) / 1000.0f;
            }

            float range_factor = 1.0f;
            switch (this->range_)
            {
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

    } // namespace adxl345_spi
} // namespace esphome