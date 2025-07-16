#include <zephyr/device.h>

#include "radio_gecko_internal.h"

/* !!! These will be configs in the future !!! */
#define PACKET_BUFFER_SIZE (1024)
#define RX_BUFFER_SIZE (1024)
#define TX_BUFFER_SIZE (1024)

struct radio_conf {

};

struct radio_api {
	int (*radio_init)(struct device* radio_dev, uint32_t configuration, uint32_t channel, radio_event_callback callback);
	void (*start_cw)(void);
	void (*stop_cw)(void);
};

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
