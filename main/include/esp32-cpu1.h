// Copyright 2022 Patrick Erley <paerley@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once
#include "inttypes.h"
#include "soc/dport_reg.h"
#include "stdbool.h"

extern volatile DRAM_ATTR uint32_t cpu1_counter;

bool set_active_bank(uint8_t bank);
bool write_set_bank(uint8_t bank, uint8_t offset, uint8_t len,
                    uint32_t *values);
bool write_clear_bank(uint8_t bank, uint8_t offset, uint8_t len,
                      uint32_t *values);

bool init_gpios(uint8_t new_banks, uint8_t new_pages);
void deinit_gpios();