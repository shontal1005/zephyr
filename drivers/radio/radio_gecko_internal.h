#include <stdint.h>

typedef void(*radio_event_callback)(void);

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

typedef struct radio_event {
	enum event_type event;
	uint8_t* data;
	int data_size;
} radio_event;
