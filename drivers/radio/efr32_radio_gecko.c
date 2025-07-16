#include "radio_gecko.h"

#include <zephyr/logging/log.h>

#include "rail.h"
#include "rail_config.h"
#include "pa_conversions_efr32.h"

radio_event_callback global_callback;

void rail_on_event_static_callback(RAIL_Handle_t rail_handle, RAIL_Events_t events)
{
	// This will handle driver related events
	// and if needed call the user callback (as last case)
	global_callback();
}

int efr32_radio_init(struct device* radio_dev, uint32_t configuration, uint16_t channel, radio_event_callback callback)
{
	radio_data* data = (radio_data*)radio_dev->data;
	global_callback = data->callback;

	RAIL_Config_t rail_config = {
		.eventsCallback = &rail_on_event_static_callback,
	};

	RAIL_Status_t status;
	int ret;

	data->rail_handle = RAIL_Init(&rail_config, NULL);
	if (!data->rail_handle) {
		LOG_ERR("RAIL_Init() failed");
	}
	status = RAIL_ConfigChannels(data->rail_handle, channelConfigs[data->channel], NULL);
	if (status) {
		LOG_ERR("RAIL_ConfigChannels(): %d", status);
	}
	status = RAIL_ConfigCal(data->rail_handle, RAIL_CAL_TEMP | RAIL_CAL_ONETIME);
	if (status) {
		LOG_ERR("RAIL_ConfigCal(): %d", status);
	}
	status = RAIL_ConfigEvents(rail_handle, RAIL_EVENTS_ALL,
				   RAIL_EVENTS_RX_COMPLETION |
				   RAIL_EVENTS_TX_COMPLETION |
				   RAIL_EVENTS_TXACK_COMPLETION |
				   RAIL_EVENT_CAL_NEEDED);
	if (status) {
		LOG_ERR("RAIL_ConfigEvents(): %d", status);
	}
	ret = RAIL_SetTxFifo(rail_handle, data->TX_buffer, 0, sizeof(data->TX_buffer)); // make sure this is the right size!!!!!!!!!
	if (ret != sizeof(data->TX_buffer)) { // make sure this is the right size!!!!!!!!!
		LOG_ERR("RAIL_SetTxFifo(): %d != %d", ret, sizeof(data->TX_buffer));
	}

	// return success!!!!!!!!!!!!
}

void efr32_start_cw(struct device* radio_dev)
{
	radio_data* data = (radio_data*)radio_dev->data;
	RAIL_StartTxStream(data->rail_handle, data->channel, RAIL_STREAM_CARRIER_WAVE);
}

void efr32_stop_cw(struct device* radio_dev)
{
	radio_data* data = (radio_data*)radio_dev->data;
	RAIL_StopTxStream(data->rail_handle);
}

static const struct radio_api efr32_radio_api = {
	.radio_init = efr32_radio_init,
	.start_cw = efr32_start_cw,
	.stop_cw = efr32_stop_cw,
};
