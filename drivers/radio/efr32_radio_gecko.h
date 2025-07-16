#include <zephyr/drivers/radio_gecko.h>

typedef struct radio_data {
	RAIL_Handle_t rail_handle;
	radio_event event;
	radio_event_callback callback;
	uint32_t configuration;
	uint16_t channel;
	uint8_t packet_buffer[PACKET_BUFFER_SIZE];
	uint8_t RX_buffer[RX_BUFFER_SIZE];
	uint8_t TX_buffer[TX_BUFFER_SIZE];
} radio_data;
