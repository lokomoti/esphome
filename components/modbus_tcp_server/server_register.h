#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/number/number.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace esphome {
namespace modbus_tcp_server {

enum RegisterType : uint8_t {
  HOLDING = 3,
  INPUT = 4,
};

enum ValueType : uint8_t {
  U_WORD,
  S_WORD,
  U_DWORD,
  S_DWORD,
  FP32,
  U_DWORD_R,
  S_DWORD_R,
  FP32_R,
};

class ServerRegister {
 public:
  ServerRegister(uint16_t address, RegisterType register_type, ValueType value_type)
      : address_(address), register_type_(register_type), value_type_(value_type) {}

  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
  void set_number(number::Number *number) { this->number_ = number; }
  void set_scale(float scale) { this->scale_ = scale; }
  void set_offset(float offset) { this->offset_ = offset; }

  uint16_t get_address() const { return this->address_; }
  RegisterType get_register_type() const { return this->register_type_; }
  ValueType get_value_type() const { return this->value_type_; }

  uint8_t register_count() const {
    switch (this->value_type_) {
      case U_WORD:
      case S_WORD:
        return 1;
      case U_DWORD:
      case S_DWORD:
      case FP32:
      case U_DWORD_R:
      case S_DWORD_R:
      case FP32_R:
        return 2;
      default:
        return 1;
    }
  }

  bool read_as_registers(std::vector<uint16_t> &out) const {
    out.clear();

    float source_value = 0.0f;
    if (!this->read_source_value_(source_value)) {
      ESP_LOGW("modbus_tcp_server", "No valid source value for addr=%u", this->address_);
      return false;
    }

    float transformed = source_value * this->scale_ + this->offset_;

    switch (this->value_type_) {
      case U_WORD: {
        uint16_t v = static_cast<uint16_t>(transformed);
        out.push_back(v);
        return true;
      }

      case S_WORD: {
        int16_t v = static_cast<int16_t>(transformed);
        out.push_back(static_cast<uint16_t>(v));
        return true;
      }

      case U_DWORD: {
        uint32_t v = static_cast<uint32_t>(transformed);
        out.push_back(static_cast<uint16_t>((v >> 16) & 0xFFFF));
        out.push_back(static_cast<uint16_t>(v & 0xFFFF));
        return true;
      }

      case S_DWORD: {
        int32_t sv = static_cast<int32_t>(transformed);
        uint32_t v = static_cast<uint32_t>(sv);
        out.push_back(static_cast<uint16_t>((v >> 16) & 0xFFFF));
        out.push_back(static_cast<uint16_t>(v & 0xFFFF));
        return true;
      }

      case FP32: {
        uint32_t raw = bit_cast<uint32_t>(transformed);
        out.push_back(static_cast<uint16_t>((raw >> 16) & 0xFFFF));
        out.push_back(static_cast<uint16_t>(raw & 0xFFFF));
        return true;
      }

      case U_DWORD_R: {
        uint32_t v = static_cast<uint32_t>(transformed);
        out.push_back(static_cast<uint16_t>(v & 0xFFFF));
        out.push_back(static_cast<uint16_t>((v >> 16) & 0xFFFF));
        return true;
      }

      case S_DWORD_R: {
        int32_t sv = static_cast<int32_t>(transformed);
        uint32_t v = static_cast<uint32_t>(sv);
        out.push_back(static_cast<uint16_t>(v & 0xFFFF));
        out.push_back(static_cast<uint16_t>((v >> 16) & 0xFFFF));
        return true;
      }

      case FP32_R: {
        uint32_t raw = bit_cast<uint32_t>(transformed);
        out.push_back(static_cast<uint16_t>(raw & 0xFFFF));
        out.push_back(static_cast<uint16_t>((raw >> 16) & 0xFFFF));
        return true;
      }
    }

    return false;
  }

 protected:
  bool read_source_value_(float &value) const {
    if (this->sensor_ != nullptr && !std::isnan(this->sensor_->state)) {
      value = this->sensor_->state;
      return true;
    }

    if (this->number_ != nullptr && !std::isnan(this->number_->state)) {
      value = this->number_->state;
      return true;
    }

    return false;
  }

  uint16_t address_;
  RegisterType register_type_;
  ValueType value_type_;
  sensor::Sensor *sensor_{nullptr};
  number::Number *number_{nullptr};
  float scale_{1.0f};
  float offset_{0.0f};
};

}  // namespace modbus_tcp_server
}  // namespace esphome