// + ----------------------------------------------------------------- +
// - File Name    : mmwavelink_esp.h
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

#ifndef MMWAVELINK_ESP_H
#define MMWAVELINK_ESP_H

#include <stdint.h>
#include "mmwavelink.h"

// User settings
#define MMWL_ESP_ENABLE_CRC16 1
#define MMWL_ESP_ENABLE_CRC32 1
#define MMWL_ESP_ENABLE_CRC64 1

#define MMWL_ESP_MAX_DEVICE_NUM 4

typedef enum
{
	MMWL_ESP_CASCADE_ROLE_SINGLE = 0x00,
	MMWL_ESP_CASCADE_ROLE_MASTER = 0x01,
	MMWL_ESP_CASCADE_ROLE_SLAVE  = 0x02
} mmwl_esp_cascade_role_e;

typedef enum
{
	// All data rates are under DDR mode
	// Note the value of data rate is:
	//  [7:4]: HSI clock rate code
	//  [3:0]: Data rate code
	MMWL_ESP_CSI_DATA_RATE_150MBPS = 0xB6,
	MMWL_ESP_CSI_DATA_RATE_225MBPS = 0x65,
	MMWL_ESP_CSI_DATA_RATE_300MBPS = 0xA4,
	MMWL_ESP_CSI_DATA_RATE_400MBPS = 0x13,
	MMWL_ESP_CSI_DATA_RATE_450MBPS = 0x52,
	MMWL_ESP_CSI_DATA_RATE_600MBPS = 0x91,
} mmwl_esp_csi_data_rate_e;

#define MMWL_ESP_CSI_DATA_RATE_CODE(RATE) (((uint32_t)(RATE)) & 0x0F)
#define MMWL_ESP_CSI_HSI_CLOCK_CODE(RATE) ((((uint32_t)(RATE)) >> 4) & 0x0F)

#define MMWL_ESP_CSI_VIRTUAL_CHANNEL(X) ((X) << 6)

typedef enum
{
	MMWL_ESP_CSI_PACKET_0_ADC_ONLY  = 0x01,
	MMWL_ESP_CSI_PACKET_0_CP_ADC    = 0x06,
	MMWL_ESP_CSI_PACKET_0_ADC_CP    = 0x09,
	MMWL_ESP_CSI_PACKET_0_CP_ADC_CQ = 0x36,
	MMWL_ESP_CSI_PACKET_0_CQ_CP_ADC = 0x39,
} mmwl_esp_csi_packet0_type_e;

typedef enum
{
	MMWL_ESP_CSI_PACKET_1_NONE  = 0x00,
	MMWL_ESP_CSI_PACKET_1_CP_CQ = 0x0E,
	MMWL_ESP_CSI_PACKET_1_CQ_CP = 0x0B,
} mmwl_esp_csi_packet1_type_e;

#define MMWL_ESP_CSI_LANE_DEFAULT  0x35421
#define MMWL_ESP_CSI_LANE_REVERSED 0x31245
#define MMWL_ESP_CSI_LANE_ID_1     0x0
#define MMWL_ESP_CSI_LANE_ID_2     0x1
#define MMWL_ESP_CSI_LANE_ID_CLK   0x4
#define MMWL_ESP_CSI_LANE_ID_3     0x2
#define MMWL_ESP_CSI_LANE_ID_4     0x3
#define MMWL_ESP_CSI_LANE_POL_NORM 0x0
#define MMWL_ESP_CSI_LANE_POL_INV  0x1
#define MMWL_ESP_CSI_LANE_CONF(lane_id, map_lane, polarity) (((map_lane) << ((lane_id) * 4)) | ((polarity) << ((lane_id) * 4 + 3)))

typedef struct
{
	// Basic device configurations
	rlCrcType_t crc_type; // Refer to RL_CRC_TYPE_*

	// RF Channel configuration
	uint32_t rx_channel_enabled;
	uint32_t tx_channel_enabled;

	// Cascade configuration
	mmwl_esp_cascade_role_e cascade_role;

	// ADC configuration
	uint32_t adc_bits;        // Refer to RL_ADC_DATA_*_BIT
	uint32_t adc_format;      // Refer to RL_ADC_FORMAT_*
	uint8_t  adc_iq_swap;     // 0: I first, 1: Q first

	// Data path configuration
	mmwl_esp_csi_packet0_type_e packet0_type; // CSI packet 0 type
	uint8_t packet0_virtual_channel;          // CSI packet 0 virtual channel
	mmwl_esp_csi_packet1_type_e packet1_type; // CSI packet 1 type
	uint8_t packet1_virtual_channel;          // CSI packet 1 virtual channel
	uint8_t cq_size[3];			              // Chirp quality size for each CQ monitor
	mmwl_esp_csi_data_rate_e  data_rate;      // CSI data rate
	uint32_t csi_lane_setting;                // CSI lane setting
} mmwl_esp_config_t;

#define MMWL_ESP_SYS_NOTIFY_NOTHING_TO_NOTIFY      0xFFFFFFFF

/// @brief A notify means the RF initialization & calibration is done
///        and the calibration report is fetched as payload
#define MMWL_ESP_SYS_NOTIFY_RF_INIT_CALIB_STATUS   0x00000000
/// @brief A notify means the frame trigger is ready
#define MMWL_ESP_SYS_NOTIFY_RF_FRAME_TRIGGER_READY 0x00000001
/// @brief A notify means the frame capture is ended
#define MMWL_ESP_SYS_NOTIFY_RF_FRAME_END           0x00000002
/// @brief A notify means the MSS (Master Sub-System, i.e. the Radar System)
///        is powered up and ready
#define MMWL_ESP_SYS_NOTIFY_DEV_MSS_POWERUP_DONE   0x00010000
/// @brief A notify means the RF module is powered up and ready
#define MMWL_ESP_SYS_NOTIFY_DEV_RF_POWERUP_DONE    0x00010001
/// @brief A notify means the CPU in MSS is under a fault condition
///        It means there is some hardware or settings problem
///        in the system and the CPU can not run properly.
///        The application should try to reset the device or
///        power cycle the device to recover from this condition.
#define MMWL_ESP_SYS_NOTIFY_DEV_MSS_CPU_FAULT      0x00010002
/// @brief A notify means there is a CRC mismatch in the message
///        from device (MSS).
#define MMWL_ESP_SYS_NOTIFY_SOFT_CRC_MISMATCHED    0x00020000
/// @brief A notify means the checksum field in message header is
///        incorrect in the message from device (MSS).
#define MMWL_ESP_SYS_NOTIFY_SOFT_CHECKSUM_ERROR    0x00020001
/// @brief A notify means the host did not receive expected interrupt
///        from device (MSS) within expected time.
#define MMWL_ESP_SYS_NOTIFY_SOFT_HOST_IRQ_TIMEOUT  0x00020002
/// @brief A notify means there is some error occurred when
///        the mmwave-link software layer is trying to use the
///        OS interface (e.g. create task, semaphore, etc.)
#define MMWL_ESP_SYS_NOTIFY_SOFT_OS_INTERFACE_ERR  0x00020003

// Pre-define the context structure
typedef struct mmwl_esp_context_t mmwl_esp_context_t;

typedef struct {
	void (*notify_async_event_isr)(uint32_t target, void* argument);
	void* async_isr_argument;
} mmwl_esp_system_notify_tunnel_t;

typedef void(*mmwl_esp_async_event_cb)(uint8_t dev_id, uint8_t* payload, uint16_t length);

typedef struct {
	/// @brief Callback when RF-initialization calibration is done
	mmwl_esp_async_event_cb init_calib_status;
	/// @brief Callback when frame trigger is ready
	mmwl_esp_async_event_cb frame_trigger_ready;
	/// @brief Callback when frame capture is ended
	mmwl_esp_async_event_cb frame_end;
} mmwl_esp_rf_ae_event_cb_t;

typedef struct {
	/// @brief Callback when MSS (Master Sub-System, i.e. the Radar System) is powered up
	mmwl_esp_async_event_cb power_up_done;
	/// @brief Callback when RF module is powered up
	mmwl_esp_async_event_cb rf_power_up_done;
	/// @brief Callback when CPU in MSS is under a fault condition
	mmwl_esp_async_event_cb cpu_fault;
} mmwl_esp_dev_event_cb_t;

typedef struct {
	/// @brief Callback when the CRC in message from device mismatches
	mmwl_esp_async_event_cb crc_mismatched;
	/// @brief Callback when the checksum field in message header is incorrect
	mmwl_esp_async_event_cb checksum_error;
	/// @brief Callback when the host did not receive expected interrupt
	///        from device within expected time
	mmwl_esp_async_event_cb host_irq_timeout;
	/// @brief Callback when there is some error occurred when
	///        the mmwave-link software layer is trying to use the
	///        OS interface (e.g. create task, semaphore, etc.)
	mmwl_esp_async_event_cb os_if_error;
} mmwl_esp_sw_event_cb_t;

typedef struct {
	/// @brief Get the current level of Host IRQ pin
	uint8_t (*get_host_irq_pin_level)(uint8_t deviceId);
	/// @brief Enable the Host IRQ
	void (*enable_host_irq)(uint8_t deviceId);

	void (*enable_device)(mmwl_esp_context_t* context, uint8_t dev_id);
	void (*disable_device)(mmwl_esp_context_t* context, uint8_t dev_id);
	void (*open_interface)(mmwl_esp_context_t* context, uint8_t dev_id);
	void (*close_interface)(mmwl_esp_context_t* context, uint8_t dev_id);

	/// @brief Write data to device by the hardware interface
	/// @note If user use SPI interface to write data, the data must be
	///       split into serval half-words (16-bit) to send.
	///       And each half-word must be little-endian format, MSB first.
	///       Also, the hardware should drive the chip-select (CS) pin cycle
	///       for each half-word (i.e. CS should not be held low for multiple
	///       half-words).
	int32_t (*write_data)(mmwl_esp_context_t* context, uint8_t dev_id, uint8_t* buffer, uint16_t length);

	/// @brief Read data from device by the hardware interface
	/// @note If user use SPI interface to read data, the data must be
	///       split into serval half-words (16-bit) to read.
	///       And each half-word must be little-endian format, MSB first.
	///       Also, the hardware should drive the chip-select (CS) pin cycle
	///       for each half-word (i.e. CS should not be held low for multiple
	///       half-words).
	int32_t (*read_data)(mmwl_esp_context_t* context, uint8_t dev_id, uint8_t* buffer, uint16_t length);
} mmwl_esp_hw_cb_t;

typedef struct {
	RL_P_EVENT_HANDLER handler;
	void* arg;
} mmwl_esp_intr_handler_t;

struct mmwl_esp_context_t {
	// A addressable device Id for handle
	uint8_t dev_id[MMWL_ESP_MAX_DEVICE_NUM];

	// Client context
	rlClientCbs_t client_context;

	// Default CRC settings
	rlCrcType_t default_crc_type; // Refer to RL_CRC_TYPE_*

	// Debug callbacks
	uint8_t dbg_level;  // Refer to RL_DBG_LEVEL_*
	rlPrintFptr dbg_print_cb;

	// Device type
	uint8_t dev_type; // Refer to RL_AR_DEVICETYPE_*

	// ACK timeout in milliseconds
	uint32_t ack_timeout_ms;

	// System notify tunnel callbacks
	// These callbacks can be implemented by the OS layer,
	// to notify the application task from asynchronous task or ISR context.
	mmwl_esp_system_notify_tunnel_t sys_notify_tunnel;

	// Asynchronous event callbacks
	mmwl_esp_rf_ae_event_cb_t rf_ae_event_cb;
	mmwl_esp_dev_event_cb_t dev_event_cb;
	mmwl_esp_sw_event_cb_t sw_event_cb;

	// Hardware control callbacks
	mmwl_esp_hw_cb_t hw_cb;
	mmwl_esp_intr_handler_t intr_handler[MMWL_ESP_MAX_DEVICE_NUM];

	// Program status flags
	void* task_wait_for_irq[MMWL_ESP_MAX_DEVICE_NUM];
};

/// @brief Connect the mmwave-link client context to callbacks in Esp context
int32_t mmwl_esp_client_connect(mmwl_esp_context_t* context);

/// @brief Pre-RF initialization
int32_t mmwl_esp_pre_rf_init(uint8_t dev_map, mmwl_esp_config_t* config);

/// @brief Post-RF initialization
int32_t mmwl_esp_post_rf_init(uint8_t dev_map, mmwl_esp_config_t* config);

/// @brief The IRQ handler to be called in the actual IRQ ISR
void mmwl_esp_host_irq_handler(mmwl_esp_context_t* context, uint8_t dev_id);

/// @brief Download the firmware image to device
int32_t mmwl_download_firmware(uint8_t dev_map, const uint8_t* firmware_img, uint32_t img_size);

#endif // MMWAVELINK_ESP_H
