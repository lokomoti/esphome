#pragma once

#include "esphome/core/log.h"
#include "server_register.h"
#include <cstdint>
#include <vector>

namespace esphome {
namespace modbus_tcp_server {

enum ModbusExceptionCode : uint8_t {
  ILLEGAL_FUNCTION = 0x01,
  ILLEGAL_DATA_ADDRESS = 0x02,
  ILLEGAL_DATA_VALUE = 0x03,
  SERVER_DEVICE_FAILURE = 0x04,
};

class RegisterBank {
 public:
  void add_register(ServerRegister *reg) {
    ESP_LOGI("modbus_tcp_server", "Register added: type=%s addr=%u width=%u",
             reg->get_register_type() == HOLDING ? "holding" : "input",
             reg->get_address(), reg->register_count());
    this->registers_.push_back(reg);
  }

  bool read_range(RegisterType type, uint16_t start_address, uint16_t count,
                  std::vector<uint16_t> &out, ModbusExceptionCode &exception) const {
    out.clear();

    ESP_LOGI("modbus_tcp_server", "Bank read_range: type=%s start=%u count=%u",
             type == HOLDING ? "holding" : "input", start_address, count);

    uint16_t current = start_address;
    uint16_t remaining = count;

    while (remaining > 0) {
      const ServerRegister *reg = this->find_at_(type, current);
      if (reg == nullptr) {
        ESP_LOGW("modbus_tcp_server", "No register found at addr=%u", current);
        exception = ILLEGAL_DATA_ADDRESS;
        out.clear();
        return false;
      }

      uint8_t width = reg->register_count();
      ESP_LOGI("modbus_tcp_server", "Found register at addr=%u width=%u", current, width);

      if (width > remaining) {
        ESP_LOGW("modbus_tcp_server", "Register width %u exceeds remaining count %u at addr=%u",
                 width, remaining, current);
        exception = ILLEGAL_DATA_ADDRESS;
        out.clear();
        return false;
      }

      std::vector<uint16_t> words;
      if (!reg->read_as_registers(words)) {
        ESP_LOGW("modbus_tcp_server", "read_as_registers() failed at addr=%u", current);
        exception = SERVER_DEVICE_FAILURE;
        out.clear();
        return false;
      }

      if (words.size() != width) {
        ESP_LOGW("modbus_tcp_server", "read_as_registers size mismatch at addr=%u expected=%u got=%u",
                 current, width, (unsigned) words.size());
        exception = SERVER_DEVICE_FAILURE;
        out.clear();
        return false;
      }

      for (auto word : words) {
        out.push_back(word);
      }

      current += width;
      remaining -= width;
    }

    return true;
  }

 protected:
  const ServerRegister *find_at_(RegisterType type, uint16_t address) const {
    for (auto *reg : this->registers_) {
      if (reg->get_register_type() != type)
        continue;
      if (reg->get_address() == address)
        return reg;
    }
    return nullptr;
  }

  std::vector<ServerRegister *> registers_;
};

}  // namespace modbus_tcp_server
}  // namespace esphome