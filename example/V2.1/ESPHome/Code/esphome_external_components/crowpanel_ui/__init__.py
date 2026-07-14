from pathlib import Path
import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.esp32 import include_builtin_idf_component
from esphome.const import CONF_ID
from esphome.core import CORE

CONF_IMAGES = "images"
CONF_FILE = "file"
CONF_RESIZE = "resize"
CONF_X = "x"
CONF_Y = "y"

DEPENDENCIES = ["esp32", "psram"]

DEFAULT_IMAGES = [
    {"file": "small_logo.png", "resize": "250x52", "x": 115, "y": 18},
    {"file": "no_light.png", "resize": "50x50", "x": 80, "y": 120},
    {"file": "light.png", "resize": "50x50", "x": 80, "y": 120},
    {"file": "small_temp.png", "resize": "50x50", "x": 215, "y": 120},
    {"file": "small_hum.png", "resize": "50x50", "x": 350, "y": 120},
]

crowpanel_ui_ns = cg.esphome_ns.namespace("crowpanel_ui")
CrowPanelUi = crowpanel_ui_ns.class_("CrowPanelUi", cg.Component)

IMAGE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_FILE): cv.string,
        cv.Required(CONF_X): cv.int_,
        cv.Required(CONF_Y): cv.int_,
        cv.Optional(CONF_RESIZE): cv.string,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CrowPanelUi),
        cv.Optional(CONF_IMAGES): cv.ensure_list(IMAGE_SCHEMA),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    include_builtin_idf_component("esp_lcd")
    cg.add_build_flag("-I$FRAMEWORK_DIR/components/esp_lcd/include")
    cg.add_build_flag("-I$FRAMEWORK_DIR/components/esp_lcd/rgb/include")

    images = config.get(CONF_IMAGES, DEFAULT_IMAGES)
    for index, entry in enumerate(images):
        data, width, height = _read_image(entry)
        symbol = f"crowpanel_rgb_image_{index}"
        cg.add_global(cg.RawStatement(_format_progmem_array(symbol, data)))
        cg.add(var.add_image_data(cg.RawExpression(symbol), width, height, entry[CONF_X], entry[CONF_Y]))


def _read_image(entry):
    try:
        from PIL import Image
    except ImportError as err:
        raise cv.Invalid("Pillow is required to convert PNG UI images") from err

    path = Path(CORE.relative_config_path(entry[CONF_FILE]))
    if not path.exists():
        raise cv.Invalid(f"UI image file not found: {path}")

    img = Image.open(path).convert("RGBA")
    if CONF_RESIZE in entry:
        width, height = _parse_resize(entry[CONF_RESIZE])
        img = img.resize((width, height))
    else:
        width, height = img.size

    data = []
    for r, g, b, a in img.getdata():
        rgb = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        data.append(rgb & 0xFF)
        data.append(rgb >> 8)
        data.append(a)
    return data, width, height


def _parse_resize(value):
    match = re.fullmatch(r"\s*(\d+)\s*x\s*(\d+)\s*", value)
    if match is None:
        raise cv.Invalid(f"Invalid resize value {value!r}, expected WIDTHxHEIGHT")
    return int(match.group(1)), int(match.group(2))


def _format_progmem_array(symbol, data):
    chunks = []
    for i in range(0, len(data), 16):
        chunks.append("  " + ", ".join(f"0x{byte:02X}" for byte in data[i : i + 16]))
    body = ",\n".join(chunks)
    return f"static const uint8_t {symbol}[] PROGMEM = {{\n{body}\n}};"
