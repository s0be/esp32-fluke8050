/**
 * Copyright 2022 Patrick Erley <paerley@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "esp32-cpu1.h"

#include <stdio.h>

#include "esp32/rom/ets_sys.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_ll.h"
#include "soc/gpio_periph.h"
#include "soc/gpio_struct.h"

void launch_cpu1();
volatile DRAM_ATTR uint32_t cpu1_counter = 0;
void DRAM_ATTR *app_cpu_stack_ptr = NULL;

// While modified from the PRO cpu, these do not need to be volatile
// as we gate modifying them across the switch callback.
volatile DRAM_ATTR uint32_t *volatile *volatile w1ts = NULL;
volatile DRAM_ATTR uint32_t *volatile *volatile w1tc = NULL;
volatile DRAM_ATTR uint8_t banks = 0;
volatile DRAM_ATTR uint8_t pages = 0;

// 0xFF means inactive
volatile DRAM_ATTR uint8_t active_bank = 0xFF;
volatile DRAM_ATTR uint8_t set_bank = 0xFF;

#define STRX(a) #a
#define STR(a) STRX(a)

#define ERR_ON(X, Y)                              \
    if (X) {                                      \
        printf("%s: %s\n", __FUNCTION__, STR(X)); \
        Y;                                        \
    }

uint32_t output_gpios = 0;
void update_gpio_directions(uint8_t bank) {
    GPIO.enable_w1tc = output_gpios;
    output_gpios = 0;
    for (int i = 0; i < pages; i++) {
        output_gpios |= w1ts[bank][i];
        output_gpios |= w1tc[bank][i];
    }
    GPIO.enable_w1ts = output_gpios;
}

bool set_active_bank(uint8_t bank) {
    ERR_ON(bank >= banks && bank != 0xFF, return false);
    set_bank = 0xFF;
    while (active_bank != 0xFF) {
        printf("Waiting for Task to stop\n");
        vTaskDelay(100);
    }
    update_gpio_directions(bank);
    set_bank = bank;
    while (active_bank != bank) {
        printf("Waiting for Task to start\n");
        vTaskDelay(100);
    }
    return true;
}

bool write_bank(volatile uint32_t *volatile *volatile bankset, uint8_t bank,
                uint8_t offset, uint8_t len, uint32_t *values) {
    ERR_ON(bank == active_bank, return false);
    ERR_ON(bank >= banks, return false);
    ERR_ON(offset > pages, return false);
    ERR_ON(offset + len > pages, return false);
    ERR_ON(offset + len < offset, return false);
    ERR_ON(values == NULL, return false);
    ERR_ON(bankset == NULL, return false);

    for (int i = offset; i < offset + len; i++) {
        bankset[bank][i] = values[i - offset];
    }
    return true;
}

bool write_set_bank(uint8_t bank, uint8_t offset, uint8_t len,
                    uint32_t *values) {
    return write_bank(w1ts, bank, offset, len, values);
}

bool write_clear_bank(uint8_t bank, uint8_t offset, uint8_t len,
                      uint32_t *values) {
    return write_bank(w1tc, bank, offset, len, values);
}

void deinit_gpios() {
    set_active_bank(0xFF);

    if (w1ts != NULL) {
        for (uint8_t i = 0; i < banks; i++) {
            if (w1ts[i] != NULL) {
                free((void *)(w1ts[i]));
                w1ts[i] = NULL;
            }
        }
        free((void *)w1ts);
        w1ts = NULL;
    }

    if (w1tc != NULL) {
        for (uint8_t i = 0; i < banks; i++) {
            if (w1tc[i] != NULL) {
                free(((void *)(w1tc[i])));
                w1tc[i] = NULL;
            }
        }
        free((void *)w1tc);
        w1tc = NULL;
    }

    banks = 0;
    pages = 0;
}

bool init_gpios(uint8_t new_banks, uint8_t new_pages) {
    deinit_gpios();
    ERR_ON(new_banks < 2, goto fail);
    ERR_ON(new_banks == 0xFF, goto fail);

    w1ts = calloc(new_banks, sizeof(uint32_t *));
    w1tc = calloc(new_banks, sizeof(uint32_t *));
    ERR_ON(w1ts == NULL || w1tc == NULL, goto fail);

    for (uint8_t i = 0; i < new_banks; i++) {
        w1ts[i] = calloc(new_pages, sizeof(uint32_t));
        w1tc[i] = calloc(new_pages, sizeof(uint32_t));
        ERR_ON(w1ts[i] == NULL || w1tc[i] == NULL, goto fail);
    }
    banks = new_banks;
    pages = new_pages;
    launch_cpu1();
    return true;

fail:
    deinit_gpios();
    return false;
}

static void IRAM_ATTR app_cpu_main(void) {
    uint8_t this_bank = set_bank;
    uint8_t idx = 0;
    while (1) {
        while (this_bank != 0xFF) {
            if (idx == pages) {
                this_bank = set_bank;
                idx = 0;
                active_bank = this_bank;
            }
            GPIO.out_w1ts = w1ts[this_bank][idx];
            GPIO.out_w1tc = w1tc[this_bank][idx];
            idx++;
            cpu1_counter++;
        }
        this_bank = set_bank;
        // TODO, do something to sleep to not just
        // burn power.
    }
}

static void IRAM_ATTR app_cpu_init() {
    // Reset the reg window. This will shift the A* registers around,
    // so we must do this in a separate ASM block.
    // Otherwise the addresses for the stack pointer and main function will be
    // invalid.
    asm volatile(
        "movi a0, 0\n"
        "wsr  a0, WindowStart\n"
        "movi a0, 0\n"
        "wsr  a0, WindowBase\n");
    // init the stack pointer and jump to main function
    asm volatile(
        "l32i a1, %0, 0\n"
        "callx4   %1\n" ::"r"(&app_cpu_stack_ptr),
        "r"(app_cpu_main));
    DPORT_REG_CLR_BIT(DPORT_APPCPU_CTRL_B_REG, DPORT_APPCPU_CLKGATE_EN);
}

void launch_cpu1() {
    if (DPORT_REG_GET_BIT(DPORT_APPCPU_CTRL_B_REG, DPORT_APPCPU_CLKGATE_EN)) {
        printf("APP CPU is already running!\n");
        return;
    }

    if (!app_cpu_stack_ptr) {
        app_cpu_stack_ptr = heap_caps_malloc(1024, MALLOC_CAP_DMA);
    }

    DPORT_REG_SET_BIT(DPORT_APPCPU_CTRL_A_REG, DPORT_APPCPU_RESETTING);
    DPORT_REG_CLR_BIT(DPORT_APPCPU_CTRL_A_REG, DPORT_APPCPU_RESETTING);

    printf("Start APP CPU at %08X\n", (uint32_t)&app_cpu_init);
    ets_set_appcpu_boot_addr((uint32_t)&app_cpu_init);
    DPORT_REG_SET_BIT(DPORT_APPCPU_CTRL_B_REG, DPORT_APPCPU_CLKGATE_EN);
}