// + ----------------------------------------------------------------- +
// - File Name    : mmwl_esp_freertos.c
// - Description  : TI MMWave-Link library porting template for FreeRTOS
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


#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "mmwl_esp_freertos.h"
#include "mmwl_esp_port.h"

// Internal implementation to spawn the worker task
typedef struct {
	RL_P_OSI_SPAWN_ENTRY entry;
	const void* arg;
} mmwl_esp_worker_task_obj_t;

#ifndef MMWL_ESP_WORKER_SPAWN_QUEUE_LEN
#define MMWL_ESP_WORKER_SPAWN_QUEUE_LEN 16
#endif

static QueueHandle_t worker_task_queue = nullptr;
static TaskHandle_t worker_spawn_task = nullptr;

static void worker_spawn_task_body(void* arg)
{
	(void)arg;
	for (;;)
	{
		mmwl_esp_worker_task_obj_t tsk_obj;
		if (xQueueReceive(worker_task_queue, &tsk_obj, portMAX_DELAY) == pdPASS)
		{
			// Execute the task
			tsk_obj.entry(tsk_obj.arg);

			// Clear the object
			// vPortFree(&taskObject);
		}
	}
}

BaseType_t mmwl_esp_worker_spawn_task_init(void)
{
	worker_task_queue = xQueueCreate(MMWL_ESP_WORKER_SPAWN_QUEUE_LEN, sizeof(mmwl_esp_worker_task_obj_t));
	BaseType_t ret = xTaskCreate(worker_spawn_task_body,
		"mmwl_spawn", configMINIMAL_STACK_SIZE * 8, NULL,
		configMAX_PRIORITIES - 1, &worker_spawn_task);

	return ret;
}

// Mutex APIs
rlInt32_t mmwl_esp_osif_mutex_create(rlOsiMutexHdl_t *mutex_handler, rlInt8_t* name)
{
	(void)name;
	*mutex_handler = (rlOsiMutexHdl_t)xSemaphoreCreateMutex();
	return RL_RET_CODE_OK;
}

rlInt32_t mmwl_esp_osif_mutex_delete(rlOsiMutexHdl_t *mutex_handler)
{
	vSemaphoreDelete((SemaphoreHandle_t)(*mutex_handler));
	return RL_RET_CODE_OK;
}

rlInt32_t mmwl_esp_osif_mutex_lock(rlOsiMutexHdl_t *mutex_handler, rlOsiTime_t timeout_ms)
{
	SemaphoreHandle_t mutex = (SemaphoreHandle_t)(*mutex_handler);
	if (xSemaphoreTake(mutex, pdMS_TO_TICKS(timeout_ms)))
	{
		return RL_RET_CODE_OK;
	}

	return RL_RET_CODE_RADAR_OSIF_ERROR;
}

rlInt32_t mmwl_esp_osif_mutex_unlock(rlOsiMutexHdl_t *mutex_handler)
{
	SemaphoreHandle_t mutex = (SemaphoreHandle_t)(*mutex_handler);
	if (xPortInIsrContext() == pdTRUE)
	{
		BaseType_t higherPriorityTaskWoken = pdFALSE;

		if (xSemaphoreGiveFromISR(mutex, &higherPriorityTaskWoken) == pdTRUE)
		{
			if (higherPriorityTaskWoken == pdTRUE)
			{
				portYIELD_FROM_ISR(higherPriorityTaskWoken);
			}

			return RL_RET_CODE_OK;
		}

		return RL_RET_CODE_RADAR_OSIF_ERROR;
	}

	if (xSemaphoreGive(mutex) == pdTRUE)
	{
		return RL_RET_CODE_OK;
	}

	return RL_RET_CODE_RADAR_OSIF_ERROR;
}

// Semaphore (binary) APIs
rlInt32_t mmwl_esp_osif_bin_sem_create(rlOsiSemHdl_t *sem_handler, rlInt8_t* name)
{
	*sem_handler = (rlOsiMutexHdl_t)xSemaphoreCreateBinary();
	return RL_RET_CODE_OK;
}

rlInt32_t mmwl_esp_osif_bin_sem_delete(rlOsiSemHdl_t *sem_handler)
{
	vSemaphoreDelete((SemaphoreHandle_t)(*sem_handler));
	return RL_RET_CODE_OK;
}

rlInt32_t mmwl_esp_osif_bin_sem_take(rlOsiSemHdl_t *sem_handler, rlOsiTime_t timeout_ms)
{
	const auto sem = (SemaphoreHandle_t)(*sem_handler);
	if (xSemaphoreTake(sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE)
	{
		return RL_RET_CODE_OK;
	}

	return RL_RET_CODE_RADAR_OSIF_ERROR;
}

rlInt32_t mmwl_esp_osif_bin_sem_give(rlOsiSemHdl_t *sem_handler)
{
	const auto sem = (SemaphoreHandle_t)(*sem_handler);
	if (xPortInIsrContext() == pdTRUE)
	{
		BaseType_t higherPriorityTaskWoken = pdFALSE;

		if (xSemaphoreGiveFromISR(sem, &higherPriorityTaskWoken) == pdTRUE)
		{
			if (higherPriorityTaskWoken == pdTRUE)
			{
				portYIELD_FROM_ISR(higherPriorityTaskWoken);
			}

			return RL_RET_CODE_OK;
		}

		return RL_RET_CODE_RADAR_OSIF_ERROR;
	}

	if (xSemaphoreGive(sem) == pdTRUE)
	{
		return RL_RET_CODE_OK;
	}

	return RL_RET_CODE_RADAR_OSIF_ERROR;
}

// Spawn task APIs
rlInt32_t mmwl_esp_osif_spawn(RL_P_OSI_SPAWN_ENTRY entry, const void* arg, rlUInt32_t flags)
{
	(void)flags;

	// Check if the worker task is initialized
	if (worker_task_queue == NULL)
	{
		return RL_RET_CODE_RADAR_OSIF_ERROR;
	}

	// Construct the task object
	mmwl_esp_worker_task_obj_t taskObject = {
		.entry = entry,
		.arg = arg
	};

	if (xPortInIsrContext() == pdTRUE)
	{
		BaseType_t higherPriorityTaskWoken = pdFALSE;
		BaseType_t retVal = xQueueSendFromISR(worker_task_queue, &taskObject, &higherPriorityTaskWoken);

		if (higherPriorityTaskWoken == pdTRUE)
		{
			portYIELD_FROM_ISR(higherPriorityTaskWoken);
		}

		if (retVal == pdPASS)
		{
			return RL_RET_CODE_OK;
		}

		// vPortFree(taskObject);
		return RL_RET_CODE_RADAR_OSIF_ERROR;
	}

	if (xQueueSend(worker_task_queue, &taskObject, portMAX_DELAY) == pdPASS)
	{
		return RL_RET_CODE_OK;
	}

	// vPortFree(taskObject);
	return RL_RET_CODE_RADAR_OSIF_ERROR;
}

// Delay API
rlInt32_t mmwl_esp_osif_delay(rlUInt32_t delay_ms)
{
	vTaskDelay(pdMS_TO_TICKS(delay_ms));
	return RL_RET_CODE_OK;
}


// Inter-task notification API
void* mmwl_esp_osif_get_current_task_handle()
{
	return xTaskGetCurrentTaskHandle();
}

void mmwl_esp_osif_wait_short_notify_from_isr(void* waiter)
{
	(void)waiter;
	(void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
}

void mmwl_esp_osif_notify_from_isr(void* target_waiter)
{
	BaseType_t higher_prio_task_woken = pdFALSE;
	vTaskNotifyGiveFromISR((TaskHandle_t)target_waiter, &higher_prio_task_woken);

	portYIELD_FROM_ISR(higher_prio_task_woken);
}
