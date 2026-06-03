import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, spi
from esphome.const import CONF_ID, CONF_RANGE

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["sensor"]

CONF_OUTPUT_DATA_RATE = "output_data_rate"
CONF_FULL_RESOLUTION = "full_resolution"
CONF_MAGNITUDE = "magnitude"
CONF_DYNAMIC_MAGNITUDE = "dynamic_magnitude"
CONF_RMS = "rms"
CONF_X = "x"
CONF_Y = "y"
CONF_Z = "z"

adxl345_spi_ns = cg.esphome_ns.namespace("adxl345_spi")

ADXL345SPIComponent = adxl345_spi_ns.class_(
    "ADXL345SPIComponent",
    cg.PollingComponent,
    spi.SPIDevice,
)

DataRate = adxl345_spi_ns.enum("DataRate")
DATA_RATES = {
    "0_10hz": DataRate.DATA_RATE_0_10,
    "0_20hz": DataRate.DATA_RATE_0_20,
    "0_39hz": DataRate.DATA_RATE_0_39,
    "0_78hz": DataRate.DATA_RATE_0_78,
    "1_56hz": DataRate.DATA_RATE_1_56,
    "3_13hz": DataRate.DATA_RATE_3_13,
    "6_25hz": DataRate.DATA_RATE_6_25,
    "12_5hz": DataRate.DATA_RATE_12_5,
    "25hz": DataRate.DATA_RATE_25,
    "50hz": DataRate.DATA_RATE_50,
    "100hz": DataRate.DATA_RATE_100,
    "200hz": DataRate.DATA_RATE_200,
    "400hz": DataRate.DATA_RATE_400,
    "800hz": DataRate.DATA_RATE_800,
    "1600hz": DataRate.DATA_RATE_1600,
    "3200hz": DataRate.DATA_RATE_3200,
}

Range = adxl345_spi_ns.enum("Range")
RANGES = {
    "2g": Range.RANGE_2G,
    "4g": Range.RANGE_4G,
    "8g": Range.RANGE_8G,
    "16g": Range.RANGE_16G,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ADXL345SPIComponent),
            cv.Optional(CONF_OUTPUT_DATA_RATE, default="100hz"): cv.enum(DATA_RATES, lower=True),
            cv.Optional(CONF_RANGE, default="4g"): cv.enum(RANGES, lower=True),
            cv.Optional(CONF_FULL_RESOLUTION, default=True): cv.boolean,
            cv.Optional(CONF_X): sensor.sensor_schema(
                unit_of_measurement="g",
                accuracy_decimals=3,
                state_class="measurement",
            ),
            cv.Optional(CONF_Y): sensor.sensor_schema(
                unit_of_measurement="g",
                accuracy_decimals=3,
                state_class="measurement",
            ),
            cv.Optional(CONF_Z): sensor.sensor_schema(
                unit_of_measurement="g",
                accuracy_decimals=3,
                state_class="measurement",
            ),
            cv.Optional(CONF_MAGNITUDE): sensor.sensor_schema(
                unit_of_measurement="g",
                accuracy_decimals=3,
                state_class="measurement",
            ),
            cv.Optional(CONF_DYNAMIC_MAGNITUDE): sensor.sensor_schema(
                unit_of_measurement="g",
                accuracy_decimals=3,
                state_class="measurement",
            ),
            cv.Optional(CONF_RMS): sensor.sensor_schema(
                unit_of_measurement="g",
                accuracy_decimals=3,
                state_class="measurement",
            ),
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield spi.register_spi_device(var, config)

    cg.add(var.set_data_rate(config[CONF_OUTPUT_DATA_RATE]))
    cg.add(var.set_range(config[CONF_RANGE]))
    cg.add(var.set_full_resolution(config[CONF_FULL_RESOLUTION]))

    if CONF_X in config:
        sens = yield sensor.new_sensor(config[CONF_X])
        cg.add(var.set_x_sensor(sens))

    if CONF_Y in config:
        sens = yield sensor.new_sensor(config[CONF_Y])
        cg.add(var.set_y_sensor(sens))

    if CONF_Z in config:
        sens = yield sensor.new_sensor(config[CONF_Z])
        cg.add(var.set_z_sensor(sens))

    if CONF_MAGNITUDE in config:
        sens = yield sensor.new_sensor(config[CONF_MAGNITUDE])
        cg.add(var.set_magnitude_sensor(sens))

    if CONF_DYNAMIC_MAGNITUDE in config:
        sens = yield sensor.new_sensor(config[CONF_DYNAMIC_MAGNITUDE])
        cg.add(var.set_dynamic_magnitude_sensor(sens))

    if CONF_RMS in config:
        sens = yield sensor.new_sensor(config[CONF_RMS])
        cg.add(var.set_rms_sensor(sens))
