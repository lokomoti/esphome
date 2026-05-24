import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_NAME, STATE_CLASS_MEASUREMENT
from . import adxl345_spi_ns, ADXL345SPIComponent, CONF_ADXL345_SPI_ID

DEPENDENCIES = ["adxl345_spi"]

ADXL345SPISensor = adxl345_spi_ns.class_(
    "ADXL345SPISensor", sensor.Sensor, cg.PollingComponent
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(ADXL345SPISensor),
            cv.GenerateID(CONF_ADXL345_SPI_ID): cv.use_id(ADXL345SPIComponent),
        }
    )
    .extend(cv.polling_component_schema("5s"))
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)

    hub = yield cg.get_variable(config[CONF_ADXL345_SPI_ID])
    cg.add(var.set_parent(hub))
