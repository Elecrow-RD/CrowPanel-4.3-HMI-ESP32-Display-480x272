idf_component_register(
    SRCS "CrowPanel_ESP32_4.3.cpp"
    INCLUDE_DIRS "."
    REQUIRES arduino-esp32 lvgl-3 UI XPT2046_Touchscreen Arduino_GFX-master
)
