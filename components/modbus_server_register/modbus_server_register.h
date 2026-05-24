#pragma once

#include "esphome/core/component.h"
#include "esphome/components/modbus_tcp_server/modbus_tcp_server.h"
#include "esphome/components/modbus_tcp_server/server_register.h"

namespace esphome {
namespace modbus_tcp_server {

class ModbusServerRegister : public Component, public ServerRegister {
 public:
  ModbusServerRegister(uint16_t address, RegisterType register_type, ValueType value_type)
      : ServerRegister(address, register_type, value_type) {}

  void set_parent(ModbusTCPServer *parent) { this->parent_ = parent; }

  void setup() override {
    if (this->parent_ != nullptr) {
      this->parent_->register_server_register(this);
    }
  }

  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  ModbusTCPServer *parent_{nullptr};
};

}  // namespace modbus_tcp_server
}  // namespace esphome