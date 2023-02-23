idf_component_register(
    SRCS
        screen/screen-core.c
        screen/screen-fluke8050.c
        tasks/task-button.c
        tasks/esp32-cpu1.c
        ttgo-xy-cp-v1.1-freertos.c
    INCLUDE_DIRS
        include/
    REQUIRES
        lvgl_esp32_drivers
        lvgl_touch
        lvgl_tft
        lvgl
        esp_adc_cal
        nvs_flash
        spi_flash
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_CONF_INCLUDE_SIMPLE=1)