#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace adxl345_spi {

enum DataRate : uint8_t {
  DATA_RATE_0_10 = 0x00,
  DATA_RATE_0_20 = 0x01,
  DATA_RATE_0_39 = 0x02,
  DATA_RATE_0_78 = 0x03,
  DATA_RATE_1_56 = 0x04,
  DATA_RATE_3_13 = 0x05,
  DATA_RATE_6_25 = 0x06,
  DATA_RATE_12_5 = 0x07,
  DATA_RATE_25 = 0x08,
  DATA_RATE_50 = 0x09,
  DATA_RATE_100 = 0x0A,
  DATA_RATE_200 = 0x0B,
  DATA_RATE_400 = 0x0C,
  DATA_RATE_800 = 0x0D,
  DATA_RATE_1600 = 0x0E,
  DATA_RATE_3200 = 0x0F,
};

enum Range : uint8_t {
  RANGE_2G = 0x00,
  RANGE_4G = 0x01,
  RANGE_8G = 0x02,
  RANGE_16G = 0x03,
};

class ADXL345SPIComponent : public PollingComponent,
                            public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST,
                                                  spi::CLOCK_POLARITY_HIGH,
                                                  spi::CLOCK_PHASE_TRAILING,
                                                  spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  void set_data_rate(DataRate data_rate) { data_rate_ = data_rate; }
  void set_range(Range range) { range_ = range; }
  void set_full_resolution(bool full_resolution) { full_resolution_ = full_resolution; }

  void set_x_sensor(sensor::Sensor *sensor) { x_sensor_ = sensor; }
  void set_y_sensor(sensor::Sensor *sensor) { y_sensor_ = sensor; }
  void set_z_sensor(sensor::Sensor *sensor) { z_sensor_ = sensor; }
  void set_magnitude_sensor(sensor::Sensor *sensor) { magnitude_sensor_ = sensor; }
  void set_dynamic_magnitude_sensor(sensor::Sensor *sensor) { dynamic_magnitude_sensor_ = sensor; }
  void set_rms_sensor(sensor::Sensor *sensor) { rms_sensor_ = sensor; }

 protected:
  uint8_t read_register_(uint8_t reg);
  void write_register_(uint8_t reg, uint8_t value);
  void read_multiple_(uint8_t reg, uint8_t *data, size_t len);
  float raw_to_g_(int16_t raw) const;
  float get_output_data_rate_hz_() const;
  void reset_window_();

  uint8_t read_fifo_sample_count_();
  void process_sample_(float x_g, float y_g, float z_g);

  DataRate data_rate_{DATA_RATE_100};
  Range range_{RANGE_4G};
  bool full_resolution_{true};

  sensor::Sensor *x_sensor_{nullptr};
  sensor::Sensor *y_sensor_{nullptr};
  sensor::Sensor *z_sensor_{nullptr};
  sensor::Sensor *magnitude_sensor_{nullptr};
  sensor::Sensor *dynamic_magnitude_sensor_{nullptr};
  sensor::Sensor *rms_sensor_{nullptr};

  float last_x_g_{0.0f};
  float last_y_g_{0.0f};
  float last_z_g_{0.0f};
  float last_magnitude_{0.0f};
  float last_dynamic_magnitude_{0.0f};

  float baseline_x_{0.0f};
  float baseline_y_{0.0f};
  float baseline_z_{0.0f};
  bool baseline_initialized_{false};

  uint32_t window_count_{0};
  float window_sum_dynamic_{0.0f};
  float window_sum_dynamic_sq_{0.0f};
  float window_peak_dynamic_{0.0f};

  float baseline_alpha_{0.01f};

  uint32_t fifo_overruns_{0};
};

}  // namespace adxl345_spi
}  // namespace esphome