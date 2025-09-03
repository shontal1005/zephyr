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

typedef enum raido_status {
    RADIO_SUCCESS = 0,
    RADIO_CONFIG_CHANNEL_FAILED,
	RADIO_START_CW_FAILED,
	RADIO_STOP_CW_FAILED,
	RADIO_WRITE_TX_FIFO_FAILED,
	RADIO_START_TX_FAILED,
	RADIO_START_RX_FAILED

} radio_status;

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
	int (*radio_init)(const struct device* radio_dev, uint32_t configuration, uint16_t channel, radio_event_callback callback);
	int (*radio_start_cw)(const struct device* radio_dev);
	int (*radio_stop_cw)(const struct device* radio_dev);
	int (*radio_send)(const struct device* radio_dev, uint16_t channel, const uint8_t* payload, int len, bool clear_fifo);
	int (*radio_start_rx_listening)(const struct device* radio_dev, uint16_t channel);

};

static inline int radio_init(const struct device* radio_dev, uint32_t configuration, uint16_t channel, radio_event_callback callback)
{
	const struct radio_api* api = (const struct radio_api*)radio_dev->api;

	return api->radio_init(radio_dev, configuration, channel, callback);
}

static inline int radio_start_cw(const struct device* radio_dev)
{
	const struct radio_api* api = (const struct radio_api*)radio_dev->api;

	return api->radio_start_cw(radio_dev);
}

static inline int radio_stop_cw(const struct device* radio_dev)
{
	const struct radio_api* api = (const struct radio_api*)radio_dev->api;

	return api->radio_stop_cw(radio_dev);
}

static inline int radio_send(const struct device* radio_dev, uint16_t channel, const uint8_t* payload, int len, bool clear_fifo)
{
	const struct radio_api* api = (const struct radio_api*)radio_dev->api;

	return api->radio_send(radio_dev, channel, payload, len, clear_fifo);
}

static inline int radio_start_rx_listening(const struct device* radio_dev, uint16_t channel)
{
	const struct radio_api* api = (const struct radio_api*)radio_dev->api;

	return api->radio_start_rx_listening(radio_dev, channel);
}
