#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "screen-core.h"
#include "sdkconfig.h"
#include "task-button.h"

// Just remove this block if you really want to build with psram support
#ifdef CONFIG_ESP32_SPIRAM_SUPPORT
#error \
    "the ttgo-xy-cp-v1.1 doesn't have PSRAM.  You'll lose heap to the psram bounce buffers"
#endif

typedef struct worker_data {
    buttons_handle_t button_data;
    display_handle_t disp_data;

    // adc_handle_t adc_data;
    // wifi_handle_t wifi_data;
} worker_data_t;

worker_data_t *alloc_data() {
    static const char *tag = "alloc_data";
    worker_data_t *wdata = calloc(1, sizeof(worker_data_t));
    if (wdata == NULL) {
        ESP_LOGE(tag, "ENOMEM allocating worker data");
        vTaskDelay(portMAX_DELAY);
    }

    wdata->button_data = init_buttons(2);

    wdata->disp_data = init_display(1);

    // voltage_worker_init(&wdata->adc_data);

    return wdata;
}

#define BUTTON1 GPIO_NUM_35
#define BUTTON2 GPIO_NUM_0
#define TFT_BL GPIO_NUM_4

int8_t screen = 0;

uint16_t brightness = 0;

void button1_evt(int64_t etime, event_t evt, button_callback_param_t parm) {
    const char *tag = "b1";
    brightness = get_brightness();
    brightness -= 400;
    if (brightness > 8192) {
        // Wraparound
        brightness = 0;
    }
    ESP_LOGI(tag, "New brightness %" PRIu16, brightness);
    set_brightness(brightness);
}

void button2_evt(int64_t etime, event_t evt, button_callback_param_t parm) {
    const char *tag = "b2";
    brightness = get_brightness();
    brightness += 400;
    if (brightness > 8192) {
        // wraparound.
        brightness = 8192;
    }
    ESP_LOGI(tag, "New brightness %" PRIu16, brightness);
    set_brightness(brightness);
}

void setup_buttons(worker_data_t *wdata) {
    button_spec_t button1 = {
        .active_level = LOW, .gpio_num = BUTTON1, .pull_mode = GPIO_FLOATING};

    int b1 = setup_button_gpio(wdata->button_data, &button1);

    button_spec_t button2 = {
        .active_level = LOW, .gpio_num = BUTTON2, .pull_mode = GPIO_FLOATING};

    int b2 = setup_button_gpio(wdata->button_data, &button2);

    button_callback_t cb1 = {.button_mask = 1 << b1,
                             .ignore_mask = 1 << b2,
                             .min_time = 100000,
                             .max_time = 2000000,
                             .release_cb = button1_evt,
                             .release_param = wdata->disp_data};

    attach_callback(wdata->button_data, &cb1);

    button_callback_t cb2 = {.button_mask = 1 << b2,
                             .ignore_mask = 1 << b1,
                             .min_time = 100000,
                             .max_time = 2000000,
                             .release_cb = button2_evt,
                             .release_param = wdata->disp_data};

    attach_callback(wdata->button_data, &cb2);
}

#include "esp32-cpu1.h"
void app_main(void) {
    static const char *tag = "main";
    ESP_LOGI(tag, "Main start");

    ESP_LOGI(tag, "Allocating objects");
    worker_data_t *wdata = alloc_data();

    ESP_LOGI(tag, "Installing Interrupt Handler");
    setup_interrupts(wdata->button_data);

    ESP_LOGI(tag, "Enabling Buttons");
    setup_buttons(wdata);

    while (1) {
        ESP_LOGI(tag, "Looping forever.");
        vTaskDelay(portMAX_DELAY);
        ESP_LOGI(tag, "Forever timed out");
    }

    free(wdata);
}