#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl/lvgl.h"

void *fluke8050_screen_init(lv_obj_t *screen);
void fluke8050_screen_worker(lv_obj_t *screen, void *priv);