#include "modbus_tcp_server.h"

namespace esphome {
namespace modbus_tcp_server {

void ModbusTCPServer::setup() {
  ESP_LOGI(TAG, "Setting up Modbus TCP Server...");
  this->ensure_server_();
}

void ModbusTCPServer::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus TCP Server:");
  ESP_LOGCONFIG(TAG, "  Port: %u", this->port_);
  ESP_LOGCONFIG(TAG, "  Unit ID: %u", this->unit_id_);
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

  if (::listen(this->listen_sock_, 1) != 0) {
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

  if (this->client_sock_ < 0) {
    this->accept_client_();
  } else {
    this->service_client_(this->client_sock_);
  }
}

void ModbusTCPServer::accept_client_() {
  struct timeval timeout {};
  timeout.tv_sec = 0;
  timeout.tv_usec = 1000;

  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(this->listen_sock_, &readfds);

  int rc = ::select(this->listen_sock_ + 1, &readfds, nullptr, nullptr, &timeout);
  if (rc <= 0) {
    return;
  }

  if (!FD_ISSET(this->listen_sock_, &readfds)) {
    return;
  }

  struct sockaddr_in source_addr {};
  socklen_t addr_len = sizeof(source_addr);
  int sock = ::accept(this->listen_sock_, reinterpret_cast<struct sockaddr *>(&source_addr), &addr_len);
  if (sock < 0) {
    return;
  }

  ESP_LOGI(TAG, "Client connected");
  this->client_sock_ = sock;

  struct timeval rw_timeout {};
  rw_timeout.tv_sec = 0;
  rw_timeout.tv_usec = 100000;
  ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &rw_timeout, sizeof(rw_timeout));
  ::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &rw_timeout, sizeof(rw_timeout));
}

bool ModbusTCPServer::recv_exact_(int sock, uint8_t *data, size_t len) {
  size_t received = 0;
  while (received < len) {
    int rc = ::recv(sock, data + received, len - received, 0);
    if (rc <= 0) {
      return false;
    }
    received += rc;
  }
  return true;
}

bool ModbusTCPServer::send_all_(int sock, const uint8_t *data, size_t len) {
  size_t sent = 0;
  while (sent < len) {
    int rc = ::send(sock, data + sent, len - sent, 0);
    if (rc <= 0) {
      return false;
    }
    sent += rc;
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
    ::close(this->client_sock_);
    this->client_sock_ = -1;
    return;
  }
  if (peek < 0) {
    return;
  }
  if (peek < 7) {
    return;
  }

  if (!this->recv_exact_(client_sock, header, 7)) {
    ESP_LOGW(TAG, "Failed to read MBAP header");
    ::close(this->client_sock_);
    this->client_sock_ = -1;
    return;
  }

  uint16_t length = (uint16_t(header[4]) << 8) | uint16_t(header[5]);
  if (length < 2 || length > 253) {
    ESP_LOGW(TAG, "Invalid MBAP length: %u", length);
    ::close(this->client_sock_);
    this->client_sock_ = -1;
    return;
  }

  // MBAP length includes unit id + PDU.
  // The 7-byte MBAP header already includes unit id, so only read the PDU remainder.
  uint16_t pdu_len = length - 1;

  std::vector<uint8_t> rest(pdu_len);
  if (pdu_len > 0 && !this->recv_exact_(client_sock, rest.data(), pdu_len)) {
    ESP_LOGW(TAG, "Failed to read request PDU");
    ::close(this->client_sock_);
    this->client_sock_ = -1;
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