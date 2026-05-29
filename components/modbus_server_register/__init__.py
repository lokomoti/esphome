import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.const import CONF_ID
from esphome.components import modbus_tcp_server, sensor, number

DEPENDENCIES = ["modbus_tcp_server"]
AUTO_LOAD = ["sensor", "number"]
MULTI_CONF = True

CONF_MODBUS_TCP_SERVER_ID = "modbus_tcp_server_id"
CONF_ADDRESS = "address"
CONF_REGISTER_TYPE = "register_type"
CONF_VALUE_TYPE = "value_type"
CONF_SENSOR_ID = "sensor_id"
CONF_NUMBER_ID = "number_id"
CONF_SCALE = "scale"
CONF_OFFSET = "offset"

modbus_server_register_ns = cg.esphome_ns.namespace("modbus_tcp_server")

ModbusServerRegister = modbus_server_register_ns.class_(
    "ModbusServerRegister", cg.Component
)

RegisterType = modbus_server_register_ns.enum("RegisterType")
ValueType = modbus_server_register_ns.enum("ValueType")

REGISTER_TYPE_OPTIONS = {
    "holding": RegisterType.HOLDING,
    "input": RegisterType.INPUT,
}

VALUE_TYPE_OPTIONS = {
    "u_word": ValueType.U_WORD,
    "s_word": ValueType.S_WORD,
    "u_dword": ValueType.U_DWORD,
    "s_dword": ValueType.S_DWORD,
    "fp32": ValueType.FP32,
    "u_dword_r": ValueType.U_DWORD_R,
    "s_dword_r": ValueType.S_DWORD_R,
    "fp32_r": ValueType.FP32_R,
}

_BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ModbusServerRegister),
        cv.Required(CONF_MODBUS_TCP_SERVER_ID): cv.use_id(
            modbus_tcp_server.ModbusTCPServer
        ),
        cv.Required(CONF_ADDRESS): cv.positive_int,
        cv.Required(CONF_REGISTER_TYPE): cv.enum(REGISTER_TYPE_OPTIONS, lower=True),
        cv.Required(CONF_VALUE_TYPE): cv.enum(VALUE_TYPE_OPTIONS, lower=True),
        cv.Optional(CONF_SENSOR_ID): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_NUMBER_ID): cv.use_id(number.Number),
        cv.Optional(CONF_SCALE, default=1.0): cv.float_,
        cv.Optional(CONF_OFFSET, default=0.0): cv.float_,
    }
).extend(cv.COMPONENT_SCHEMA)


def _validate_source(config):
    has_sensor = CONF_SENSOR_ID in config
    has_number = CONF_NUMBER_ID in config
    if has_sensor == has_number:
        raise cv.Invalid(
            "Exactly one of sensor_id or number_id must be provided")
    return config


CONFIG_SCHEMA = cv.All(_BASE_SCHEMA, _validate_source)


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_ADDRESS],
        config[CONF_REGISTER_TYPE],
        config[CONF_VALUE_TYPE],
    )
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_MODBUS_TCP_SERVER_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_scale(config[CONF_SCALE]))
    cg.add(var.set_offset(config[CONF_OFFSET]))

    if CONF_SENSOR_ID in config:
        sens = await cg.get_variable(config[CONF_SENSOR_ID])
        cg.add(var.set_sensor(sens))

    if CONF_NUMBER_ID in config:
        num = await cg.get_variable(config[CONF_NUMBER_ID])
        cg.add(var.set_number(num))
