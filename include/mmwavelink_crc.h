// + ----------------------------------------------------------------- +
// - File Name    : mmwavelink_crc.h
// - Description  : CRC calculation functions for TI MMWave-Link
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

#ifndef MMWAVELINK_CRC_H
#define MMWAVELINK_CRC_H

#include <stdint.h>
#include "mmwavelink_esp.h"
#include "mmwavelink.h"

#if ((!MMWL_ESP_ENABLE_CRC16) && (!MMWL_ESP_ENABLE_CRC32) && (!MMWL_ESP_ENABLE_CRC64))
#error "At least one type of CRC (16/32/64) should be enabled!"
#endif

rlInt32_t mmwl_esp_calculate_crc(rlUInt8_t* data, rlUInt32_t length, rlUInt8_t type, rlUInt8_t* crcBuffer);

#endif // MMWAVELINK_CRC_H
