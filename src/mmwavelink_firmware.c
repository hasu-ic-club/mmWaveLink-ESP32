// + ----------------------------------------------------------------- +
// - File Name    : mmwavelink_firmware.c
// - Description  : Firmware related functions for TI MMWave-Link
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
#include <string.h>

#define FIRMWARE_CHUNK_MAX_SIZE 232U
#define FIRMWARE_HEADER_SIZE 8U
#define FIRMWARE_FIRST_CHUNK_SIZE 224U

static rlFileData_t g_firmware_file = { 0 };

static rlInt32_t esp_write_firmware_chunk(rlUInt8_t dev_map, rlUInt16_t rem_chunks, rlUInt16_t chunk_len, rlUInt8_t* data)
{
	// Copy the chunk data to the global structure
	g_firmware_file.chunkLen = chunk_len;
	memcpy(g_firmware_file.fData, data, chunk_len);

	return rlDeviceFileDownload(dev_map, &g_firmware_file, rem_chunks);
}

int32_t mmwl_download_firmware(uint8_t dev_map, const uint8_t* firmware_img, const uint32_t img_size)
{
	const uint8_t* p_frame = (uint8_t*)firmware_img;
	constexpr uint16_t chunk_len = FIRMWARE_CHUNK_MAX_SIZE;
	const uint32_t chunks = (img_size + FIRMWARE_HEADER_SIZE) / chunk_len;
	uint32_t rem_chunks = chunks;
	uint32_t last_chunk_len, first_chunk_len;

	if (rem_chunks > 0)
	{
		last_chunk_len = (img_size + FIRMWARE_HEADER_SIZE) % chunk_len;
		first_chunk_len = FIRMWARE_CHUNK_MAX_SIZE;
		rem_chunks += 1; // Add one chunk for the header
	}
	else
	{
		first_chunk_len = img_size + FIRMWARE_HEADER_SIZE;
	    last_chunk_len = 0;
	}

	// Set the header chunk
	uint32_t* pHeader = (uint32_t*)(&g_firmware_file.fData[0]);
	pHeader[0] = (uint32_t)0;
	pHeader[1] = img_size;

	// Copy the first chunk data
	memcpy((uint8_t*)(&pHeader[2]), p_frame, first_chunk_len - FIRMWARE_HEADER_SIZE);

	// Write the firmware chunk
	g_firmware_file.chunkLen = first_chunk_len;
	rlInt32_t retVal = rlDeviceFileDownload(dev_map, &g_firmware_file, rem_chunks);

	if (retVal != RL_RET_CODE_OK)
	{
		return retVal;
	}

	// Move the pointer to next chunk
	p_frame += FIRMWARE_FIRST_CHUNK_SIZE;
	rem_chunks--;

	while (rem_chunks > 0)
	{
		if ((rem_chunks == 1) && (last_chunk_len > 0))
		{
			// Last chunk
			memcpy((uint8_t*)(&g_firmware_file.fData[0]), p_frame, last_chunk_len);
			g_firmware_file.chunkLen = last_chunk_len;
			retVal = rlDeviceFileDownload(dev_map, &g_firmware_file, 0);

			if (retVal != RL_RET_CODE_OK)
			{
				return retVal;
			}
		}
		else
		{
			memcpy((uint8_t*)(&g_firmware_file.fData[0]), p_frame, FIRMWARE_CHUNK_MAX_SIZE);
			g_firmware_file.chunkLen = FIRMWARE_CHUNK_MAX_SIZE;
			retVal = rlDeviceFileDownload(dev_map, &g_firmware_file, rem_chunks - 1);

			if (retVal != RL_RET_CODE_OK)
			{
				return retVal;
			}

			// Move the pointer to next chunk
			p_frame += FIRMWARE_CHUNK_MAX_SIZE;
		}

		rem_chunks--;
	}

	return retVal;
}