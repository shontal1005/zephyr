#pragma once

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

} radio_status_t;

typedef struct radio_event {

	/* The event that accured */
	enum event_type event;

	/* The data received if exist */
	uint8_t* data;

	/* Data buffer size */
	int data_size;

} radio_event;

typedef void(*radio_event_callback)(struct radio_event* event);

struct radio_conf {
};

struct radio_api {
	int (*radio_init)(const struct device* radio_dev, uint32_t configuration, uint16_t channel, radio_event_callback callback);
	int (*radio_start_cw)(void);
	int (*radio_stop_cw)(void);
	int (*radio_send)(uint16_t channel, const uint8_t* payload, int len, bool clear_fifo);
	int (*radio_start_rx_listening)(uint16_t channel);
};

extern const struct radio_api* radio_driver_api;

static inline int radio_init(const struct device* radio_dev, uint32_t configuration, uint16_t channel, radio_event_callback callback)
{
	struct radio_api* api = (struct radio_api*)radio_dev->api;

	return api->radio_init(radio_dev, configuration, channel, callback);
}

static inline int radio_start_cw()
{
	return radio_driver_api->radio_start_cw();
}

static inline int radio_stop_cw()
{
	return radio_driver_api->radio_stop_cw();
}

static inline int radio_send(uint16_t channel, const uint8_t* payload, int len, bool clear_fifo)
{
	return radio_driver_api->radio_send(channel, payload, len, clear_fifo);
}

static inline int radio_start_rx_listening(uint16_t channel)
{
	return radio_driver_api->radio_start_rx_listening(channel);
}
