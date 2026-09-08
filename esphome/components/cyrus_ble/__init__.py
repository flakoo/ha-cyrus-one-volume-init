import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, light
from esphome.const import CONF_ID

DEPENDENCIES = ['binary_sensor', 'light']
AUTO_LOAD = ['binary_sensor']

cyrus_ble_ns = cg.esphome_ns.namespace('cyrus_ble')
CyrusBleComponent = cyrus_ble_ns.class_('CyrusBleComponent', cg.Component)

CONF_STATUS_SENSOR = "status_sensor"
CONF_LED = "led"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(CyrusBleComponent),
    cv.Optional(CONF_STATUS_SENSOR): cv.use_id(binary_sensor.BinarySensor),
    cv.Optional(CONF_LED): cv.use_id(light.LightState),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_STATUS_SENSOR in config:
        sens = await cg.get_variable(config[CONF_STATUS_SENSOR])
        cg.add(var.set_status_sensor(sens))

    if CONF_LED in config:
        led = await cg.get_variable(config[CONF_LED])
        cg.add(var.set_led(led))
