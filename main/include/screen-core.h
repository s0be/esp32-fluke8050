#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl/lvgl.h"

typedef enum display_mode {
    FLUKE_8050A = 0,
    MAX_DISPLAY_MODE = FLUKE_8050A
} display_mode_t;

typedef void *screen_handle_t;
typedef void *display_handle_t;

typedef void (*tick_callback_t)(lv_obj_t *screen, void *priv);

void set_brightness(uint16_t brightness);
uint16_t get_brightness();
display_handle_t init_display();