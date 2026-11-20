// + ----------------------------------------------------------------- +
// - File Name    : mmwl_esp_port.h
// - Description  : TI MMWave-Link library porting abstraction layer
// - Author       : Onodera Tsusaki
// - Date Created : 2025/09
// - Organization : Hasunosora IC Design Club.
// - Version      : 1.0
// + ----------------------------------------------------------------- +
// - Change Log   :
//   Date          Author            Version    Description
//   2025/9/1      Onodera T.        1.0        Initial version
// + ----------------------------------------------------------------- +
// Copyright (C) 2025 Hasunosora IC Design Club.
// All rights reserved.
// This file is part of the mmwavelink-esp32 library.
// + ----------------------------------------------------------------- +

#ifndef _MMWL_ESP_PORT_H_
#define _MMWL_ESP_PORT_H_

#include <stdint.h>
#include "mmwavelink.h"

// Mutex porting APIs
rlInt32_t mmwl_esp_osif_mutex_create(rlOsiMutexHdl_t *mutex_handler, rlInt8_t* name);
rlInt32_t mmwl_esp_osif_mutex_delete(rlOsiMutexHdl_t *mutex_handler);
rlInt32_t mmwl_esp_osif_mutex_lock(rlOsiMutexHdl_t *mutex_handler, rlOsiTime_t timeout_ms);
rlInt32_t mmwl_esp_osif_mutex_unlock(rlOsiMutexHdl_t *mutex_handler);

// Semaphore (binary) porting APIs
rlInt32_t mmwl_esp_osif_bin_sem_create(rlOsiSemHdl_t *sem_handler, rlInt8_t* name);
rlInt32_t mmwl_esp_osif_bin_sem_delete(rlOsiSemHdl_t *sem_handler);
rlInt32_t mmwl_esp_osif_bin_sem_take(rlOsiSemHdl_t *sem_handler, rlOsiTime_t timeout_ms);
rlInt32_t mmwl_esp_osif_bin_sem_give(rlOsiSemHdl_t *sem_handler);

// Spawn task APIs
rlInt32_t mmwl_esp_osif_spawn(RL_P_OSI_SPAWN_ENTRY entry, const void* arg, rlUInt32_t flags);

// Delay API
rlInt32_t mmwl_esp_osif_delay(rlUInt32_t delay_ms);

// Inter-task notification API
void* mmwl_esp_osif_get_current_task_handle();
void mmwl_esp_osif_wait_short_notify_from_isr(void* waiter);
void mmwl_esp_osif_notify_from_isr(void* target_waiter);

#endif //_MMWL_ESP_PORT_H_
