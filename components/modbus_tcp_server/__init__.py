import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT

DEPENDENCIES = ["network"]
MULTI_CONF = False

CONF_UNIT_ID = "unit_id"
CONF_MAX_CLIENTS = "max_clients"

modbus_tcp_server_ns = cg.esphome_ns.namespace("modbus_tcp_server")

ModbusTCPServer = modbus_tcp_server_ns.class_("ModbusTCPServer", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ModbusTCPServer),
        cv.Optional(CONF_PORT, default=502): cv.port,
        cv.Optional(CONF_UNIT_ID, default=1): cv.int_range(min=1, max=255),
        cv.Optional(CONF_MAX_CLIENTS, default=4): cv.int_range(min=1, max=16),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_unit_id(config[CONF_UNIT_ID]))
    cg.add(var.set_max_clients(config[CONF_MAX_CLIENTS]))
