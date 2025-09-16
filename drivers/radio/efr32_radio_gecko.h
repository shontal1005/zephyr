#pragma once

#include <zephyr/drivers/radio_gecko.h>

struct radio_data {
	RAIL_Handle_t rail_handle;
	radio_event event;
	radio_event_callback callback;
	uint32_t configuration;
	uint16_t channel;
	uint8_t payload_to_send[CONFIG_RADIO_EFR32_TX_BUFFER_SIZE];
	uint8_t received_packet_buffer[CONFIG_RADIO_EFR32_RX_BUFFER_SIZE];
	uint8_t TX_buffer[CONFIG_RADIO_EFR32_TX_BUFFER_SIZE] __aligned(4);
	// uint8_t RX_buffer[CONFIG_RADIO_EFR32_RX_BUFFER_SIZE] __aligned(4);
} __packed;
