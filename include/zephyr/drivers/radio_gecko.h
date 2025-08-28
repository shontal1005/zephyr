#include <stdint.h>

#include <zephyr/device.h>

#include "rail.h"

/* !!! These will be configs in the future !!! */
#define PACKET_BUFFER_SIZE (1024)

enum event_type {

	/* RX ended successfully */
	RX_DONE,

	/* Error accured while trying to receive data */
	RX_ERROR,

	/* TX ended successfully */
	TX_DONE,

	/* Error accured while trying to send data */
	TX_ERROR,

	/* RSSI ended successfully, the data can be collected */
	RSSI_DONE,

	/* Number of events exist */
	NUMBER_OF_EVENTS
};

typedef void(*radio_event_callback)(void);
typedef void (*radio_irq_config_func)(const struct device* dev);

typedef struct radio_event {
	enum event_type event;
	uint8_t* data;
	int data_size;
} radio_event;

struct radio_conf {
	radio_irq_config_func irq_config_func;
};

struct radio_api {
	int (*radio_init)(struct device* radio_dev, uint32_t configuration, uint16_t channel, radio_event_callback callback);
	void (*radio_start_cw)(struct device* radio_dev);
	void (*radio_stop_cw)(struct device* radio_dev);
};

static inline int radio_init(struct device* radio_dev, uint32_t configuration, uint16_t channel, radio_event_callback callback)
{
	const struct radio_api* api = (const struct radio_api*)radio_dev->api;

	return api->radio_init(radio_dev, configuration, channel, callback);
}

static inline void radio_start_cw(struct device* radio_dev)
{
	const struct radio_api* api = (const struct radio_api*)radio_dev->api;

	api->radio_start_cw(radio_dev);
}

static inline void radio_stop_cw(struct device* radio_dev)
{
	const struct radio_api* api = (const struct radio_api*)radio_dev->api;

	api->radio_stop_cw(radio_dev);
}
