# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

# src/CMakeLists.txt
idf_component_register(
    SRCS
        "main.c"
        "ble_nus.c"
        "SSD1306_MINI.c"
        "BMP_180.c"
        "ble_handler.c"
        "control.c"
        "display.c"
        "logger.c"
        "sensor.c"
        "state_machine.c"
        "time_manager.c"
    
    INCLUDE_DIRS "."
    REQUIRES driver nvs_flash bt esp_hw_support
)
