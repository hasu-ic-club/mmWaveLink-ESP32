// + ----------------------------------------------------------------- +
// - File Name    : mmwl_esp_freertos.h
// - Description  : Header of TI MMWave-Link library porting template for FreeRTOS
// - Author       : Onodera Tsusaki
// - Date Created : 2025/09
// - Company      : Hasunosora IC Design Club.
// - Version      : 1.2
// + ----------------------------------------------------------------- +
// - Change Log   :
//   Date          Author            Version    Description
//   2025/9/30     Onodera T.        1.0        Initial version
//   2026/3/2      Onodera T.        1.1        Add macro definitions for
//                                              different platforms
//   2026/3/2      Onodera T.        1.2		Add a entry/exit function
//                                              for critical ISR flag
//                                              on Microblaze platform
// + ----------------------------------------------------------------- +
// Copyright (C) 2025-2026 Hasunosora IC Design Club.
// All rights reserved.
// This file is part of the mmwavelink-esp32 library.
// + ----------------------------------------------------------------- +

#ifndef MMWL_ESP_FREERTOS_H
#define MMWL_ESP_FREERTOS_H

#include "portmacro.h"

BaseType_t mmwl_esp_worker_spawn_task_init();

#endif // MMWL_ESP_FREERTOS_H
