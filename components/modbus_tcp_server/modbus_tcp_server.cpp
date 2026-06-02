#include "modbus_tcp_server.h"
#include <algorithm>

namespace esphome {
namespace modbus_tcp_server {

void ModbusTCPServer::setup() {
  ESP_LOGI(TAG, "Setting up Modbus TCP Server...");
  this->ensure_server_();
  this->publish_connected_clients_if_changed_();
}

void ModbusTCPServer::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus TCP Server:");
  ESP_LOGCONFIG(TAG, "  Port: %u", this->port_);
  ESP_LOGCONFIG(TAG, "  Unit ID: %u", this->unit_id_);
  ESP_LOGCONFIG(TAG, "  Max clients: %u", this->max_clients_);
  LOG_SENSOR("  ", "Connected clients", this->connected_clients_sensor_);
}

bool ModbusTCPServer::ensure_server_() {
  if (this->listen_sock_ >= 0) {
    return true;
  }

  this->listen_sock_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (this->listen_sock_ < 0) {
    ESP_LOGE(TAG, "Failed to create socket: errno=%d", errno);
    return false;
  }

  int opt = 1;
  ::setsockopt(this->listen_sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  // Make the listening socket non-blocking so accept() returns immediately
  // when no pending connections are queued.
  int flags = ::fcntl(this->listen_sock_, F_GETFL, 0);
  ::fcntl(this->listen_sock_, F_SETFL, flags | O_NONBLOCK);

  struct sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(this->port_);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (::bind(this->listen_sock_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0) {
    ESP_LOGE(TAG, "Bind failed on port %u: errno=%d", this->port_, errno);
    ::close(this->listen_sock_);
    this->listen_sock_ = -1;
    return false;
  }

  if (::listen(this->listen_sock_, 5) != 0) {
    ESP_LOGE(TAG, "Listen failed: errno=%d", errno);
    ::close(this->listen_sock_);
    this->listen_sock_ = -1;
    return false;
  }

  ESP_LOGI(TAG, "Listening on TCP port %u", this->port_);
  return true;
}

void ModbusTCPServer::loop() {
  if (!this->ensure_server_()) {
    return;
  }

  this->accept_client_();

  // Snapshot the current client list before iterating so that a close inside
  // service_client_() does not invalidate the in-progress iteration.
  std::vector<int> to_service = this->client_socks_;
  for (int sock : to_service) {
    this->service_client_(sock);
  }

  this->publish_connected_clients_if_changed_();
}

void ModbusTCPServer::publish_connected_clients_if_changed_() {
  if (this->connected_clients_sensor_ == nullptr) {
    return;
  }

  int32_t current = static_cast<int32_t>(this->client_socks_.size());
  if (current == this->last_connected_clients_) {
    return;
  }

  this->last_connected_clients_ = current;
  this->connected_clients_sensor_->publish_state(current);
}

void ModbusTCPServer::accept_client_() {
  // Drain all pending connections up to max_clients_ in one loop pass.
  // The listening socket is non-blocking, so accept() returns -1/EAGAIN
  // immediately when no more connections are queued.
  while (this->client_socks_.size() < this->max_clients_) {
    struct sockaddr_in source_addr {};
    socklen_t addr_len = sizeof(source_addr);
    int sock = ::accept(this->listen_sock_, reinterpret_cast<struct sockaddr *>(&source_addr), &addr_len);
    if (sock < 0) {
      break;  // EAGAIN/EWOULDBLOCK: no pending connections
    }

    // Set client socket to non-blocking so recv/send never stall the loop.
    int flags = ::fcntl(sock, F_GETFL, 0);
    ::fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    this->client_socks_.push_back(sock);
    ESP_LOGI(TAG, "Client connected (%u active)", (unsigned) this->client_socks_.size());
  }
}

void ModbusTCPServer::close_client_(int sock) {
  ::close(sock);
  auto it = std::find(this->client_socks_.begin(), this->client_socks_.end(), sock);
  if (it != this->client_socks_.end()) {
    this->client_socks_.erase(it);
  }
  ESP_LOGI(TAG, "Client closed (%u remaining)", (unsigned) this->client_socks_.size());
}

bool ModbusTCPServer::recv_exact_(int sock, uint8_t *data, size_t len) {
  size_t received = 0;
  while (received < len) {
    int rc = ::recv(sock, data + received, len - received, 0);
    if (rc > 0) {
      received += rc;
    } else if (rc == 0) {
      return false;  // connection closed by peer
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;  // no data yet; spin (Modbus messages are small and arrive quickly)
      }
      return false;
    }
  }
  return true;
}

bool ModbusTCPServer::send_all_(int sock, const uint8_t *data, size_t len) {
  size_t sent = 0;
  while (sent < len) {
    int rc = ::send(sock, data + sent, len - sent, 0);
    if (rc > 0) {
      sent += rc;
    } else if (rc == 0) {
      return false;
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;  // send buffer temporarily full; spin
      }
      return false;
    }
  }
  return true;
}

bool ModbusTCPServer::parse_request_(const uint8_t *buf, size_t len, ModbusRequest &req) {
  if (len < 12) {
    ESP_LOGW(TAG, "Request too short: %u bytes", (unsigned) len);
    return false;
  }

  req.transaction_id = (uint16_t(buf[0]) << 8) | uint16_t(buf[1]);
  req.protocol_id = (uint16_t(buf[2]) << 8) | uint16_t(buf[3]);
  req.length = (uint16_t(buf[4]) << 8) | uint16_t(buf[5]);
  req.unit_id = buf[6];
  req.function_code = buf[7];
  req.address = (uint16_t(buf[8]) << 8) | uint16_t(buf[9]);
  req.quantity = (uint16_t(buf[10]) << 8) | uint16_t(buf[11]);

  return true;
}

void ModbusTCPServer::send_exception_(int sock, const ModbusRequest &req, ModbusExceptionCode ex) {
  ESP_LOGW(TAG, "Sending exception fc=0x%02X ex=0x%02X", req.function_code, (uint8_t) ex);

  uint8_t resp[9];
  resp[0] = (req.transaction_id >> 8) & 0xFF;
  resp[1] = req.transaction_id & 0xFF;
  resp[2] = 0x00;
  resp[3] = 0x00;
  resp[4] = 0x00;
  resp[5] = 0x03;
  resp[6] = req.unit_id;
  resp[7] = req.function_code | 0x80;
  resp[8] = static_cast<uint8_t>(ex);

  this->send_all_(sock, resp, sizeof(resp));
}

void ModbusTCPServer::handle_read_registers_(int sock, const ModbusRequest &req, RegisterType type) {
  ESP_LOGI(TAG, "Read request type=%s addr=%u qty=%u",
           type == HOLDING ? "holding" : "input",
           req.address, req.quantity);

  if (req.quantity == 0 || req.quantity > 125) {
    this->send_exception_(sock, req, ILLEGAL_DATA_VALUE);
    return;
  }

  std::vector<uint16_t> regs;
  ModbusExceptionCode ex = SERVER_DEVICE_FAILURE;

  if (!this->bank_.read_range(type, req.address, req.quantity, regs, ex)) {
    this->send_exception_(sock, req, ex);
    return;
  }

  uint8_t byte_count = regs.size() * 2;
  std::vector<uint8_t> resp;
  resp.reserve(9 + byte_count);

  resp.push_back((req.transaction_id >> 8) & 0xFF);
  resp.push_back(req.transaction_id & 0xFF);
  resp.push_back(0x00);
  resp.push_back(0x00);
  resp.push_back(0x00);
  resp.push_back(uint8_t(3 + byte_count));
  resp.push_back(req.unit_id);
  resp.push_back(req.function_code);
  resp.push_back(byte_count);

  for (auto reg : regs) {
    resp.push_back((reg >> 8) & 0xFF);
    resp.push_back(reg & 0xFF);
  }

  ESP_LOGI(TAG, "Sending %u registers (%u bytes)", (unsigned) regs.size(), (unsigned) byte_count);
  this->send_all_(sock, resp.data(), resp.size());
}

void ModbusTCPServer::service_client_(int client_sock) {
  uint8_t header[7];

  int peek = ::recv(client_sock, header, sizeof(header), MSG_PEEK);
  if (peek == 0) {
    ESP_LOGI(TAG, "Client disconnected");
    this->close_client_(client_sock);
    return;
  }
  if (peek < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;  // no data available this loop iteration
    }
    this->close_client_(client_sock);
    return;
  }
  if (peek < 7) {
    return;  // partial header; wait for more data
  }

  if (!this->recv_exact_(client_sock, header, 7)) {
    ESP_LOGW(TAG, "Failed to read MBAP header");
    this->close_client_(client_sock);
    return;
  }

  uint16_t length = (uint16_t(header[4]) << 8) | uint16_t(header[5]);
  if (length < 2 || length > 253) {
    ESP_LOGW(TAG, "Invalid MBAP length: %u", length);
    this->close_client_(client_sock);
    return;
  }

  // MBAP length includes unit id + PDU.
  // The 7-byte MBAP header already includes unit id, so only read the PDU remainder.
  uint16_t pdu_len = length - 1;

  std::vector<uint8_t> rest(pdu_len);
  if (pdu_len > 0 && !this->recv_exact_(client_sock, rest.data(), pdu_len)) {
    ESP_LOGW(TAG, "Failed to read request PDU");
    this->close_client_(client_sock);
    return;
  }

  std::vector<uint8_t> frame;
  frame.reserve(7 + pdu_len);
  for (size_t i = 0; i < 7; i++) frame.push_back(header[i]);
  for (size_t i = 0; i < pdu_len; i++) frame.push_back(rest[i]);

  ModbusRequest req{};
  if (!this->parse_request_(frame.data(), frame.size(), req)) {
    return;
  }

  ESP_LOGI(TAG, "Request: tx=%u unit=%u fc=0x%02X addr=%u qty=%u",
           req.transaction_id, req.unit_id, req.function_code, req.address, req.quantity);

  if (req.protocol_id != 0) {
    ESP_LOGW(TAG, "Unsupported protocol id: %u", req.protocol_id);
    return;
  }

  if (req.unit_id != this->unit_id_) {
    ESP_LOGW(TAG, "Ignoring request for unit id %u (configured %u)", req.unit_id, this->unit_id_);
    return;
  }

  switch (req.function_code) {
    case 0x03:
      this->handle_read_registers_(client_sock, req, HOLDING);
      break;
    case 0x04:
      this->handle_read_registers_(client_sock, req, INPUT);
      break;
    default:
      this->send_exception_(client_sock, req, ILLEGAL_FUNCTION);
      break;
  }
}

}  // namespace modbus_tcp_server
}  // namespace esphome