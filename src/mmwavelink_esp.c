// + ----------------------------------------------------------------- +
// - File Name    : mmwavelink_esp.c
// - Description  : TI MMWave-Link for ESP32 (RISC-V series) host
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

#include "mmwavelink_esp.h"
#include "mmwl_esp_port.h"
#include "mmwavelink_crc.h"

#include <string.h>

#define MMWL_ESP_DEVICE_MAP_FROM_ID(DEVID) (1 << (DEVID))

static mmwl_esp_context_t* managed_handle;

static void esp_rf_async_event_cb(mmwl_esp_context_t* context, uint8_t deviceId, uint16_t sb_id,
	mmwl_esp_async_event_cb* user_cb, uint32_t* sys_notify)
{
	*user_cb = nullptr;
	*sys_notify = MMWL_ESP_SYS_NOTIFY_NOTHING_TO_NOTIFY;

	switch (sb_id)
	{
	case RL_RF_AE_INITCALIBSTATUS_SB:
		// RF-initialization calibration done, reported status
		*user_cb = context->rf_ae_event_cb.init_calib_status;
		*sys_notify = MMWL_ESP_SYS_NOTIFY_RF_INIT_CALIB_STATUS;
		break;
	case RL_RF_AE_FRAME_TRIGGER_RDY_SB:
		// Frame trigger is ready
		*user_cb = context->rf_ae_event_cb.frame_trigger_ready;
		*sys_notify = MMWL_ESP_SYS_NOTIFY_RF_FRAME_TRIGGER_READY;
		break;
	case RL_RF_AE_FRAME_END_SB:
		// Frame capture is ended
		*user_cb = context->rf_ae_event_cb.frame_end;
		*sys_notify = MMWL_ESP_SYS_NOTIFY_RF_FRAME_END;
		break;
	}
}

static void esp_dev_event_cb(mmwl_esp_context_t* context, uint8_t dev_id, uint16_t sb_id,
	mmwl_esp_async_event_cb* user_cb, uint32_t* sys_notify)
{
	*user_cb = nullptr;
	*sys_notify = MMWL_ESP_SYS_NOTIFY_NOTHING_TO_NOTIFY;

	switch (sb_id)
	{
	case RL_DEV_AE_MSSPOWERUPDONE_SB:
		*user_cb = context->dev_event_cb.power_up_done;
		*sys_notify = MMWL_ESP_SYS_NOTIFY_DEV_MSS_POWERUP_DONE;
		break;
	case RL_DEV_AE_RFPOWERUPDONE_SB:
		*user_cb = context->dev_event_cb.rf_power_up_done;
		*sys_notify = MMWL_ESP_SYS_NOTIFY_DEV_RF_POWERUP_DONE;
		break;
	case RL_DEV_AE_MSS_CPUFAULT_SB:
		*user_cb = context->dev_event_cb.cpu_fault;
		*sys_notify = MMWL_ESP_SYS_NOTIFY_DEV_MSS_CPU_FAULT;
		break;
	}
}

static void esp_sw_event_cb(mmwl_esp_context_t* context, uint8_t dev_id, uint16_t sb_id, uint8_t* payload,
	mmwl_esp_async_event_cb* user_cb, uint32_t* sys_notify)
{
	*user_cb = nullptr;
	*sys_notify = MMWL_ESP_SYS_NOTIFY_NOTHING_TO_NOTIFY;

	switch (sb_id)
	{
	case RL_MMWL_AE_MISMATCH_REPORT:
		uint32_t errorCode = *(uint32_t*)payload;
		switch (errorCode)
		{
		case RL_RET_CODE_CRC_FAILED:
			*user_cb = context->sw_event_cb.crc_mismatched;
			*sys_notify = MMWL_ESP_SYS_NOTIFY_SOFT_CRC_MISMATCHED;
			break;
		case RL_RET_CODE_CHKSUM_FAILED:
			*user_cb = context->sw_event_cb.checksum_error;
			*sys_notify = MMWL_ESP_SYS_NOTIFY_SOFT_CHECKSUM_ERROR;
			break;
		case RL_RET_CODE_HOSTIRQ_TIMEOUT:
			*user_cb = context->sw_event_cb.host_irq_timeout;
			*sys_notify = MMWL_ESP_SYS_NOTIFY_SOFT_HOST_IRQ_TIMEOUT;
			break;
		case RL_RET_CODE_RADAR_OSIF_ERROR:
			*user_cb = context->sw_event_cb.os_if_error;
			*sys_notify = MMWL_ESP_SYS_NOTIFY_SOFT_OS_INTERFACE_ERR;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

static void esp_async_event_cb(uint8_t dev_id, uint16_t sb_id, uint16_t sb_len, uint8_t* payload)
{
	const uint16_t msg_id = sb_id / RL_MAX_SB_IN_MSG;
	const uint16_t sub_block_id = RL_GET_SBID_FROM_MSG(sb_id, msg_id);
	mmwl_esp_context_t* tgt_ctx = managed_handle;

	mmwl_esp_async_event_cb user_cb;
	uint32_t sys_notify;

	switch (msg_id)
	{
	case RL_RF_ASYNC_EVENT_MSG:
		// Asynchronous event from RF sub-system
		esp_rf_async_event_cb(tgt_ctx, dev_id, sub_block_id,
			&user_cb, &sys_notify);
		break;
	case RL_DEV_ASYNC_EVENT_MSG:
		// Asynchronous event from device (MSS)
		esp_dev_event_cb(tgt_ctx, dev_id, sub_block_id,
			&user_cb, &sys_notify);
		break;
	case RL_MMWL_ASYNC_EVENT_MSG:
		// Asynchronous event from MMWave-Link (software layer)
		esp_sw_event_cb(tgt_ctx, dev_id, sub_block_id, payload,
			&user_cb, &sys_notify);
		break;
	}

	// Call the user callback if registered
	if (user_cb)
	{
		user_cb(dev_id, payload, sb_len);
	}

	// Send system notify if needed
	if ((sys_notify != MMWL_ESP_SYS_NOTIFY_NOTHING_TO_NOTIFY) &&
		(tgt_ctx->sys_notify_tunnel.notify_async_event_isr))
	{
		tgt_ctx->sys_notify_tunnel.notify_async_event_isr(sys_notify,
					tgt_ctx->sys_notify_tunnel.async_isr_argument);
	}
}

static rlInt32_t esp_register_intr_handler(uint8_t dev_id, RL_P_EVENT_HANDLER handler, void* arg)
{
	// Register the interrupt handler
	managed_handle->intr_handler[dev_id].handler = handler;
	managed_handle->intr_handler[dev_id].arg = arg;

	// Enable the host IRQ
	managed_handle->hw_cb.enable_host_irq(dev_id);

	return RL_RET_CODE_OK;
}

static rlInt32_t esp_wait_intr_status(rlComIfHdl_t if_handler, rlUInt8_t low_high)
{
	// The comIfHdl is equal to the deviceId
	const uint8_t dev_id = *(uint8_t*)if_handler;
	mmwl_esp_context_t* context = managed_handle;

	// Get the current task handle and set to the context
	// This is used to notify the task when the interrupt occurs
	context->task_wait_for_irq[dev_id] = mmwl_esp_osif_get_current_task_handle();

	// Get current IRQ level
	uint8_t irq_level = context->hw_cb.get_host_irq_pin_level(dev_id);

	// If the level is already the expected level, return immediately
	if (irq_level == low_high)
	{
		context->task_wait_for_irq[dev_id] = NULL;
		return irq_level;
	}

	// Else, wait for the IRQ changed to the expected level
	do {
		// Wait for the notification from ISR
		mmwl_esp_osif_wait_short_notify_from_isr(context->task_wait_for_irq[dev_id]);

		// Update the IRQ level
		irq_level = context->hw_cb.get_host_irq_pin_level(dev_id);
	} while (irq_level != low_high);

	// Clean the waiting task handle
	context->task_wait_for_irq[dev_id] = NULL;

	return irq_level;
}

static void esp_nothing_to_do_with_if_handler(rlComIfHdl_t if_handler)
{
	(void)if_handler;
	// Nothing to do
}

static rlInt32_t esp_enable_device(rlUInt8_t dev_id)
{
	mmwl_esp_context_t* context = managed_handle;
	if (context->hw_cb.enable_device)
	{
		context->hw_cb.enable_device(context, dev_id);
	}

	return RL_RET_CODE_OK;
}

static rlInt32_t esp_disable_device(rlUInt8_t dev_id)
{
	mmwl_esp_context_t* context = managed_handle;
	if (context->hw_cb.disable_device)
	{
		context->hw_cb.disable_device(context, dev_id);
	}

	return RL_RET_CODE_OK;
}

static rlComIfHdl_t esp_interface_open(rlUInt8_t dev_id, rlUInt32_t flags)
{
	(void)flags;
	mmwl_esp_context_t* context = managed_handle;

	if (context->hw_cb.open_interface)
	{
		context->hw_cb.open_interface(context, dev_id);
	}

	// Put the device ID to object instance for future use
	context->dev_id[dev_id] = dev_id;
	return (rlComIfHdl_t)&context->dev_id[dev_id];
}

static rlInt32_t esp_interface_close(rlComIfHdl_t if_handler)
{
	// The comIfHdl is equal to the deviceId
	const uint8_t dev_id = *(uint8_t*)if_handler;
	mmwl_esp_context_t* context = managed_handle;

	if (context->hw_cb.close_interface)
	{
		context->hw_cb.close_interface(context, dev_id);
	}

	return RL_RET_CODE_OK;
}

static rlInt32_t esp_interface_write(rlComIfHdl_t if_handler, rlUInt8_t* buffer, rlUInt16_t length)
{
	// The comIfHdl is equal to the deviceId
	const uint8_t dev_id = *(uint8_t*)if_handler;
	mmwl_esp_context_t* context = managed_handle;

	if (context->hw_cb.write_data)
	{
		return context->hw_cb.write_data(context, dev_id, buffer, length);
	}

	return RL_RET_CODE_FATAL_ERROR;
}

static rlInt32_t esp_interface_read(rlComIfHdl_t if_handler, rlUInt8_t* buffer, rlUInt16_t length)
{
	// The comIfHdl is equal to the deviceId
	const uint8_t deviceId = *(uint8_t*)if_handler;
	mmwl_esp_context_t* context = managed_handle;

	if (context->hw_cb.read_data)
	{
		return context->hw_cb.read_data(context, deviceId, buffer, length);
	}

	return RL_RET_CODE_FATAL_ERROR;
}

int32_t mmwl_esp_client_connect(mmwl_esp_context_t* context)
{
	// Set managed handle
	managed_handle = context;

	// Register the callbacks required by mmwave-link library
	rlClientCbs_t* ctx = &context->client_context;

	// Step I: Communication interface callbacks
	ctx->comIfCb.rlComIfOpen = esp_interface_open;
	ctx->comIfCb.rlComIfClose = esp_interface_close;
	ctx->comIfCb.rlComIfWrite = esp_interface_write;
	ctx->comIfCb.rlComIfRead = esp_interface_read;

	// Step II: Device control callbacks
	ctx->devCtrlCb.rlDeviceEnable = esp_enable_device;
	ctx->devCtrlCb.rlDeviceDisable = esp_disable_device;
	//  Nothing to do for these callbacks because we are using edge-triggered interrupt
	ctx->devCtrlCb.rlDeviceMaskHostIrq = esp_nothing_to_do_with_if_handler;
	ctx->devCtrlCb.rlDeviceUnMaskHostIrq = esp_nothing_to_do_with_if_handler;
	ctx->devCtrlCb.rlDeviceWaitIrqStatus = esp_wait_intr_status;
	ctx->devCtrlCb.rlRegisterInterruptHandler = esp_register_intr_handler;

	// Step III: Asynchronous event handler
	ctx->eventCb.rlAsyncEvent = esp_async_event_cb;

	// Step IV: OS Interfaces
	ctx->osiCb.mutex.rlOsiMutexCreate = mmwl_esp_osif_mutex_create;
	ctx->osiCb.mutex.rlOsiMutexDelete = mmwl_esp_osif_mutex_delete;
	ctx->osiCb.mutex.rlOsiMutexLock = mmwl_esp_osif_mutex_lock;
	ctx->osiCb.mutex.rlOsiMutexUnLock = mmwl_esp_osif_mutex_unlock;

	ctx->osiCb.sem.rlOsiSemCreate = mmwl_esp_osif_bin_sem_create;
	ctx->osiCb.sem.rlOsiSemDelete = mmwl_esp_osif_bin_sem_delete;
	ctx->osiCb.sem.rlOsiSemWait = mmwl_esp_osif_bin_sem_take;
	ctx->osiCb.sem.rlOsiSemSignal = mmwl_esp_osif_bin_sem_give;

	ctx->osiCb.queue.rlOsiSpawn = mmwl_esp_osif_spawn;
	ctx->timerCb.rlDelay = mmwl_esp_osif_delay;

	// Step V: Implementation of CRC calculation
	ctx->crcCb.rlComputeCRC = mmwl_esp_calculate_crc;
	ctx->crcType = context->default_crc_type;

	// Step VI: Implement debug interface
	ctx->dbgCb.rlPrint = context->dbg_print_cb;
	ctx->dbgCb.dbgLevel = context->dbg_level;

	// Step VII: Initialize the mmwave-link driver
	ctx->platform = RL_PLATFORM_HOST;
	ctx->arDevType = context->dev_type;

	// Step VIII: Set ACK timeout to enable ACK mechanism
	ctx->ackTimeout = context->ack_timeout_ms;

	return RL_RET_CODE_OK;
}

int32_t mmwl_esp_pre_rf_init(uint8_t dev_map, mmwl_esp_config_t* config)
{
	int32_t ret;

	// 1. Static configuration
	// 1.1 Channel
	// NOTE: For cascading, here is the setting of pinout
	//    - [0] 20G CLKOUT(MST):  ON (0)
	//    - [1] 20G SYNCOUT(MST): ON (0)
	//    - [2] 20G CLKOUT(SLV):  OFF(0)
	//    - [3] 20G SYNCOUT(SLV): OFF(0)
	//    - [4] Local oscillitor: Loopback (0)
	//    - [5] OSC CLK:          ON (0)
	//    - [6] FRC Sync In:      Loopback (0) <Master ONLY>

	rlChanCfg_t ch_config = {
		.rxChannelEn = config->rx_channel_enabled,
		.txChannelEn = config->tx_channel_enabled,
		.cascading = (rlUInt16_t)config->cascade_role,
		.cascadingPinoutCfg = 0b0000000
	};

	ret = rlSetChannelConfig(dev_map, &ch_config);

	if (ret != RL_RET_CODE_OK)
	{
		return ret;
	}

	// 1.2 ADC
	rlAdcOutCfg_t adc_out_cfg = {
		.fmt.b2AdcBits = config->adc_bits,
		.fmt.b2AdcOutFmt = config->adc_format,
		.fmt.b8FullScaleReducFctr = 0
	};

	ret = rlSetAdcOutConfig(dev_map, &adc_out_cfg);

	if (ret != RL_RET_CODE_OK)
	{
		return ret;
	}

	// 1.3 Data format
	rlDevDataFmtCfg_t data_fmt_cfg = {
		.adcBits = config->adc_bits, //Use the same bit config for 'Data format' and 'ADC'
		.rxChannelEn = config->rx_channel_enabled,
		.iqSwapSel = config->adc_iq_swap,
		.chInterleave = 0,
	};

	// Use the 'Complex' for 'Complex1x/Complex2x' mode
	// and use the 'Real' for 'Real' mode
	if ((config->adc_format == RL_ADC_FORMAT_REAL) || (config->adc_format == RL_ADC_FORMAT_PSEUDO_REAL))
	{
		data_fmt_cfg.adcFmt = 0x00; // Real
	}
	else
	{
		data_fmt_cfg.adcFmt = 0x01; // Complex
	}

	ret = rlDeviceSetDataFmtConfig(dev_map, &data_fmt_cfg);

	if (ret != RL_RET_CODE_OK)
	{
		return ret;
	}

	// 1.4 Enable RF LDO bypass (1.0V supply direct to chip)
	rlRfLdoBypassCfg_t ldo_bypass_cfg = {
		// Enable RF LDO bypass/Disable PA LDO input
		// This configuration is for only 1.0V RF supply is provided,
		// and VIN_RF2 is shorted to VOUT_PA (1.0V)
		.ldoBypassEnable = 0b11,
		.supplyMonIrDrop = 0,
		.ioSupplyIndicator = 0,  // 3.3V IO (TODO: Check if 1.8V IO is supported)
	};

	ret = rlRfSetLdoBypassConfig(dev_map, &ldo_bypass_cfg);

	if (ret != RL_RET_CODE_OK)
	{
		return ret;
	}

	// 1.5 LP ADC Mode
	rlLowPowerModeCfg_t lp_mode_cfg = {
		.lpAdcMode = 0x00, // Disable LP ADC mode
	};

	ret = rlSetLowPowerModeConfig(dev_map, &lp_mode_cfg);

	if (ret != RL_RET_CODE_OK)
	{
		return ret;
	}

	// 1.6 Asynchronous event direction
	rlRfDevCfg_t rf_dev_cfg = {
		// [1:0]: Global asynchronous event
		// [3:2]: Monitoring asynchronous event
		.aeDirection = 0b0101, // Both events are radar sub-system to host
		.aeCrcConfig = config->crc_type
	};

	ret = rlRfSetDeviceCfg(dev_map, &rf_dev_cfg);

	if (ret != RL_RET_CODE_OK)
	{
		return ret;
	}

	// Now all pre-RF initialization is done
	// User can run 'RF Initialization' command now
	return ret;
}

int32_t mmwl_esp_post_rf_init(uint8_t dev_map, mmwl_esp_config_t* config)
{
	int32_t ret;

	// 2. Device data path configuration
	// 2.1 Data path configuration
	rlDevDataPathCfg_t dp_cfg = {
		.intfSel = 0, // For production uses, only CSI-2 is supported
		.transferFmtPkt0 = (uint8_t)(config->packet0_type) | (uint8_t)(config->packet0_virtual_channel),
		.transferFmtPkt1 = (uint8_t)(config->packet1_type) | (uint8_t)(config->packet1_virtual_channel),
		.cqConfig = config->adc_bits,
		.cq0TransSize = config->cq_size[0],
		.cq1TransSize = config->cq_size[1],
		.cq2TransSize = config->cq_size[2],
	};

	ret = rlDeviceSetDataPathConfig(dev_map, &dp_cfg);

	if (ret != RL_RET_CODE_OK)
	{
		return ret;
	}

	// 2.2 Data path clock configuration
	rlDevDataPathClkCfg_t dc_clk_cfg = {
		.laneClkCfg = 0x01, // DDR for CSI-2 interface
		.dataRate = MMWL_ESP_CSI_DATA_RATE_CODE(config->data_rate),
	};

	ret = rlDeviceSetDataPathClkConfig(dev_map, &dc_clk_cfg);

	if (ret != RL_RET_CODE_OK)
	{
		return ret;
	}

	// 2.3 High speed interface (HSI) clock configuration
	rlDevHsiClk_t dev_hsi_cfg = {
		.hsiClk = MMWL_ESP_CSI_HSI_CLOCK_CODE(config->data_rate),
	};

	ret = rlDeviceSetHsiClk(dev_map, &dev_hsi_cfg);

	if (ret != RL_RET_CODE_OK)
	{
		return ret;
	}

	// 2.4 CSI configuration
	rlDevCsi2Cfg_t csi_cfg = {
		.lanePosPolSel = config->csi_lane_setting,
		.lineStartEndDis = 0x00, // Enable line start/end
	};

	ret = rlDeviceSetCsi2Config(dev_map, &csi_cfg);

	if (ret != RL_RET_CODE_OK)
	{
		return ret;
	}

	// Now all sensor-unrelated post-RF initialization is done
	// User can setup the profile, chirp and frame configurations now
	// After all configurations are done, user can run 'Start Frame' command to start the radar operation
	return ret;
}

void mmwl_esp_host_irq_handler(mmwl_esp_context_t* context, uint8_t dev_id)
{
	if (dev_id >= MMWL_ESP_MAX_DEVICE_NUM)
	{
		// Invalid device ID, ignore the interrupt
		return;
	}

	if (context->hw_cb.get_host_irq_pin_level(dev_id))
	{
		// Host IRQ pin is high, notify the mmwave-link library
		if (context->intr_handler[dev_id].handler)
		{
			context->intr_handler[dev_id].handler(dev_id, context->intr_handler[dev_id].arg);
		}
	}

	if (context->task_wait_for_irq[dev_id])
	{
		mmwl_esp_osif_notify_from_isr(context->task_wait_for_irq[dev_id]);
	}
}
