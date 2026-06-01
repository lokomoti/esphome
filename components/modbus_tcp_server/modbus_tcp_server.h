#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "register_bank.h"

#ifdef USE_ESP32
#include <errno.h>
#include <fcntl.h>
#include <lwip/inet.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <sys/select.h>
#endif

#include <vector>

namespace esphome {
namespace modbus_tcp_server {

static const char *const TAG = "modbus_tcp_server";

struct ModbusRequest {
  uint16_t transaction_id;
  uint16_t protocol_id;
  uint16_t length;
  uint8_t unit_id;
  uint8_t function_code;
  uint16_t address;
  uint16_t quantity;
};

class ModbusTCPServer : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_port(uint16_t port) { this->port_ = port; }
  void set_unit_id(uint8_t unit_id) { this->unit_id_ = unit_id; }
  void set_max_clients(uint8_t max_clients) { this->max_clients_ = max_clients; }

  void register_server_register(ServerRegister *reg) { this->bank_.add_register(reg); }

 protected:
  bool ensure_server_();
  void accept_client_();
  void service_client_(int client_sock);
  void close_client_(int sock);

  bool recv_exact_(int sock, uint8_t *data, size_t len);
  bool send_all_(int sock, const uint8_t *data, size_t len);

  bool parse_request_(const uint8_t *mbap_pdu, size_t len, ModbusRequest &req);
  void send_exception_(int sock, const ModbusRequest &req, ModbusExceptionCode ex);
  void handle_read_registers_(int sock, const ModbusRequest &req, RegisterType type);

  uint16_t port_{502};
  uint8_t unit_id_{1};
  uint8_t max_clients_{4};

  int listen_sock_{-1};
  std::vector<int> client_socks_;

  RegisterBank bank_;
};

}  // namespace modbus_tcp_server
}  // namespace esphome