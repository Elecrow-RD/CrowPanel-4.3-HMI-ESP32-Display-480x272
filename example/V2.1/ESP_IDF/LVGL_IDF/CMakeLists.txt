# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS components/UI components/XPT2046_Touchscreen components/Arduino_GFX-master)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(CrowPanel_ESP32_4.3)

# GCC 14.x may ICE on some esp-dsp conv sources during -O2 optimization.
idf_component_get_property(esp_dsp_lib espressif__esp-dsp COMPONENT_LIB)
if(esp_dsp_lib)
    set(_esp_dsp_ice_sources
        "${CMAKE_SOURCE_DIR}/managed_components/espressif__esp-dsp/modules/conv/float/dspi_conv_f32_ansi.c"
        "${CMAKE_SOURCE_DIR}/managed_components/espressif__esp-dsp/modules/conv/float/dsps_conv_f32_ansi.c"
        "${CMAKE_SOURCE_DIR}/managed_components/espressif__esp-dsp/modules/conv/float/dsps_corr_f32_ansi.c"
        "${CMAKE_SOURCE_DIR}/managed_components/espressif__esp-dsp/modules/conv/float/dsps_ccorr_f32_ansi.c"
    )
    set_source_files_properties(
        ${_esp_dsp_ice_sources}
        TARGET_DIRECTORY ${esp_dsp_lib}
        PROPERTIES COMPILE_OPTIONS "-O0"
    )
endif()
