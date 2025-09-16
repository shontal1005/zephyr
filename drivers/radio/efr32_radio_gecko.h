#pragma once

#include <zephyr/drivers/radio_gecko.h>

struct radio_data {
	/* RAIL handle instance */
	RAIL_Handle_t rail_handle;

	/* Event for the user to use in callback*/
	radio_event event;

	/* The user callback to be called on events */
	radio_event_callback callback;

	/* The radio configuration to use */
	uint32_t configuration;

	/* The radio channel to use */
	uint16_t channel;

	/* Buffer to hold the variable length packet (packet and size) */
	uint8_t payload_to_send[CONFIG_RADIO_EFR32_TX_BUFFER_SIZE];

	/* Buffer to hold the received packet */
	uint8_t received_packet_buffer[CONFIG_RADIO_EFR32_RX_BUFFER_SIZE];

	/* Buffer given for RAIL to use for transmiting */
	uint8_t TX_buffer[CONFIG_RADIO_EFR32_TX_BUFFER_SIZE] __aligned(4);

	/* Buffers given for RAIL to use for receiving */
	uint8_t RX_buffer[CONFIG_RADIO_EFR32_RX_BUFFER_SIZE] __aligned(4);
} __packed;
