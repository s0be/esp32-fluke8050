#include "screen-core.h"

#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl_tft/st7789.h"
#include "screen-fluke8050.h"

#define TFT_MOSI GPIO_NUM_19
#define TFT_SCLK GPIO_NUM_18
#define TFT_CS GPIO_NUM_5
#define TFT_DC GPIO_NUM_16
#define TFT_RST GPIO_NUM_23
#define TFT_BL GPIO_NUM_4

typedef struct screen_data {
    lv_obj_t *screen;
    tick_callback_t tick_cb;
    tick_callback_t unload_cb;
    tick_callback_t load_cb;
    void *priv;
} screen_data_t;

typedef struct display_content_worker_data {
    uint8_t mode;
    QueueHandle_t display_event_queue;
    lv_style_t *my_style;

    uint8_t screen_cnt;
    screen_data_t *screen;
} display_content_worker_data_t;

typedef struct display_data {
    TaskHandle_t display_task;
    QueueHandle_t display_event_queue;
    display_content_worker_data_t *workerdata;
} display_data_t;

static const char *display_tag = "display_tag";

void tick_task(void *arg) { lv_tick_inc(10); }

void display_content_worker(lv_task_t *param) {
    display_content_worker_data_t *wdata =
        (display_content_worker_data_t *)param->user_data;
    display_mode_t new_mode = FLUKE_8050A;
    if (xQueueReceive(wdata->display_event_queue, &new_mode, 0) == pdTRUE) {
        if (new_mode != wdata->mode) {
            lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
            if (wdata->mode < new_mode) {
                anim = LV_SCR_LOAD_ANIM_MOVE_LEFT;
            }

            lv_scr_load_anim(wdata->screen[new_mode].screen, anim, 100, 10,
                             false);

            if (wdata->screen[wdata->mode].unload_cb != NULL) {
                wdata->screen[wdata->mode].unload_cb(
                    wdata->screen[wdata->mode].screen,
                    wdata->screen[wdata->mode].priv);
            }

            if (wdata->screen[new_mode].load_cb != NULL) {
                wdata->screen[new_mode].load_cb(wdata->screen[new_mode].screen,
                                                wdata->screen[new_mode].priv);
            }

            wdata->mode = new_mode;
        }
    }

    if (wdata->screen[wdata->mode].tick_cb != NULL) {
        wdata->screen[wdata->mode].tick_cb(wdata->screen[wdata->mode].screen,
                                           wdata->screen[wdata->mode].priv);
    }
}

void show_display(display_handle_t disp_handle, display_mode_t disp) {
    display_data_t *ddata = (display_data_t *)disp_handle;
    xQueueSend(ddata->display_event_queue, &disp, pdMS_TO_TICKS(100));
}

static uint16_t brightness = 4096;
void set_brightness(uint16_t newbrightness) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, newbrightness);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    brightness = newbrightness;
}
uint16_t get_brightness() { return brightness; }

void display_worker(void *param) {
    display_content_worker_data_t *dwdata = param;

    ESP_LOGI(display_tag, "Initializing Display");
    lv_init();
    lvgl_driver_init();

#define DISPLAY_BUF_SIZE (LV_HOR_RES_MAX * 40)
    ESP_LOGI(display_tag, "Initializing Framebuffers for %ix%i display",
             CONFIG_LV_DISPLAY_WIDTH, CONFIG_LV_DISPLAY_HEIGHT);

    static lv_color_t *buf[2];
    buf[0] = calloc(DISPLAY_BUF_SIZE, sizeof(lv_color_t));
    buf[1] = calloc(DISPLAY_BUF_SIZE, sizeof(lv_color_t));

    lv_disp_buf_t *disp_buf = calloc(1, sizeof(lv_disp_buf_t));
    lv_disp_buf_init(disp_buf, buf[0], buf[1], DISPLAY_BUF_SIZE);

    lv_disp_drv_t *display_drv = calloc(1, sizeof(lv_disp_drv_t));
    lv_disp_drv_init(display_drv);

    display_drv->flush_cb = st7789_flush;
    display_drv->buffer = disp_buf;
    lv_disp_drv_register(display_drv);

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &tick_task, .name = "gui_tick_task"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000));

    dwdata->mode = FLUKE_8050A;

    lv_style_t *style = calloc(1, sizeof(lv_style_t));

    dwdata->my_style = style;
    lv_style_init(style);
    lv_style_set_text_color(style, LV_STATE_DEFAULT, LV_COLOR_GREEN);
    lv_style_set_bg_color(style, LV_STATE_DEFAULT, LV_COLOR_BLACK);

    lv_obj_t *fluke8050_screen = lv_obj_create(NULL, NULL);
    lv_obj_add_style(fluke8050_screen, LV_OBJ_PART_MAIN, style);
    lv_obj_set_size(fluke8050_screen, CONFIG_LV_DISPLAY_WIDTH,
                    CONFIG_LV_DISPLAY_HEIGHT);
    dwdata->screen[0].priv = fluke8050_screen_init(fluke8050_screen);
    dwdata->screen[0].screen = fluke8050_screen;
    dwdata->screen[0].tick_cb = fluke8050_screen_worker;

    lv_scr_load(dwdata->screen[0].screen);
    lv_task_t *task =
        lv_task_create(display_content_worker, 100, LV_TASK_PRIO_LOW, dwdata);

    ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                      .timer_num = LEDC_TIMER_0,
                                      .duty_resolution = LEDC_TIMER_13_BIT,
                                      .freq_hz = 1000,
                                      .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t bl_pwm = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                    .channel = LEDC_CHANNEL_0,
                                    .intr_type = LEDC_INTR_DISABLE,
                                    .timer_sel = LEDC_TIMER_0,
                                    .gpio_num = TFT_BL,
                                    .duty = 0,
                                    .hpoint = 0};

    ESP_ERROR_CHECK(ledc_channel_config(&bl_pwm));

    set_brightness(brightness);

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_task_handler();
    }

    lv_task_del(task);
}

display_handle_t init_display(int screen_count) {
    display_content_worker_data_t *dwdata =
        calloc(1, sizeof(display_content_worker_data_t));
    if (dwdata == NULL) {
        ESP_LOGE(display_tag, "Failed to create dwdata");
        vTaskDelay(portMAX_DELAY);
    }

    dwdata->display_event_queue = xQueueCreate(10, sizeof(display_mode_t));
    if (dwdata->display_event_queue == NULL) {
        ESP_LOGE(display_tag, "Failed to create display_event_queue");
        vTaskDelay(portMAX_DELAY);
    }

    display_data_t *ddata = calloc(1, sizeof(display_data_t));
    if (ddata == NULL) {
        ESP_LOGE(display_tag, "Failed to create ddata");
        vTaskDelay(portMAX_DELAY);
    }

    ddata->workerdata = dwdata;
    ddata->display_event_queue = dwdata->display_event_queue;

    ddata->workerdata->screen_cnt = screen_count;
    ddata->workerdata->screen = calloc(screen_count, sizeof(screen_data_t));

    if (ddata->workerdata->screen == NULL) {
        ESP_LOGE(display_tag, "Failed to create the ddata->workerdata->screen");
        vTaskDelay(portMAX_DELAY);
    }

    BaseType_t ret = xTaskCreate(&display_worker, display_tag, 4 * 1024, dwdata,
                                 3, &ddata->display_task);
    if (ret != pdTRUE) {
        ESP_LOGE(display_tag, "Failed to create the display_task");
        vTaskDelay(portMAX_DELAY);
    }

    return ddata;
}