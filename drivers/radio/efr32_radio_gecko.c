#define DT_DRV_COMPAT efr32_radio_gecko
#include <stdio.h>
#include "efr32_radio_gecko.h"

#include <zephyr/logging/log.h>
#include <zephyr/irq.h>

#include "xg28_rb4401c/rail_config.h"
#include "pa_conversions_efr32.h"

radio_event_callback global_callback;

LOG_MODULE_REGISTER(driver, 4);

void rail_on_event_static_callback(RAIL_Handle_t rail_handle, RAIL_Events_t events)
{
	// This will handle driver related events
	// and if needed call the user callback (as last case)
	global_callback();
}

int efr32_radio_init(struct device* radio_dev, uint32_t configuration, uint16_t channel, radio_event_callback callback)
{
	RAIL_Status_t status;

	radio_data* data = (radio_data*)radio_dev->data;
	data->callback = callback;
	data->channel = channel;

	global_callback = data->callback;



	LOG_INF("config channels: %d", (uint32_t)status);
	// return success!!!!!!!!!!!!
	return 0;
}

void efr32_radio_start_cw(struct device* radio_dev)
{
	radio_data* data = (radio_data*)radio_dev->data;
	RAIL_StartTxStream(data->rail_handle, data->channel, RAIL_STREAM_CARRIER_WAVE);
}

void efr32_radio_stop_cw(struct device* radio_dev)
{
	radio_data* data = (radio_data*)radio_dev->data;
	RAIL_StopTxStream(data->rail_handle);
}

static const struct radio_api efr32_radio_api = {
	.radio_init = efr32_radio_init,
	.radio_start_cw = efr32_radio_start_cw,
	.radio_stop_cw = efr32_radio_stop_cw,
};

static void rail_on_rf_ready(RAIL_Handle_t rail_handle)
{
	LOG_INF("radio is ready %p", rail_handle);
}

static void rail_on_channel_config(RAIL_Handle_t rail_handle,
				   const RAIL_ChannelConfigEntry_t *entry)
{
	LOG_INF("in config funciton");
	sl_rail_util_pa_on_channel_config_change(rail_handle, entry);
}

void rail_on_event(RAIL_Handle_t rail_handle, RAIL_Events_t events)
{
	RAIL_Status_t status;

	if (events & RAIL_EVENTS_RX_COMPLETION) {
		if (events & RAIL_EVENT_RX_PACKET_RECEIVED) {
			LOG_INF("rx completion");

		} else {
			LOG_ERR("radio rx error: %08llx", events);
		}
	}

	if (events & RAIL_EVENTS_TX_COMPLETION) {
		if (!(events & RAIL_EVENT_TX_PACKET_SENT)) {
			LOG_ERR("radio tx error: %08llx", events);
		}
		LOG_INF("tx completion");

	}

	if (events & RAIL_EVENTS_TXACK_COMPLETION) {
		/* We do not configure Tx ack. Catch the event anyway */
		LOG_INF("received ack completion");
	}

	if (events & RAIL_EVENT_CAL_NEEDED) {
		status = RAIL_Calibrate(rail_handle, NULL, RAIL_CAL_ALL_PENDING);
		if (status) {
			LOG_ERR("RAIL_Calibrate(): %d", status);
		}
	}
}

static int efr32_radio_initialization(const struct device* dev)
{
	sl_rail_util_pa_init();

	static uint8_t tx_fifo[256] __aligned(4);
	RAIL_Config_t rail_config = {
		.eventsCallback = &rail_on_event,
	};
	RAIL_DataConfig_t data_config = {
		.txSource = TX_PACKET_DATA,
		.rxSource = RX_PACKET_DATA,
		.txMethod = PACKET_MODE,
		.rxMethod = PACKET_MODE,
	};
	RAIL_StateTransitions_t transitions = {
		.success = RAIL_RF_STATE_RX,
		.error   = RAIL_RF_STATE_RX,
	};
	RAIL_Handle_t rail_handle;
	RAIL_Status_t status;
	int ret;

	rail_handle = RAIL_Init(&rail_config, &rail_on_rf_ready);
	if (!rail_handle) {
		LOG_ERR("RAIL_Init() failed");
	}
	status = RAIL_ConfigData(rail_handle, &data_config);
	if (status) {
		LOG_ERR("RAIL_ConfigData(): %d", status);
	}
	status = RAIL_ConfigChannels(rail_handle, channelConfigs[0], &rail_on_channel_config);
	if (status) {
		LOG_ERR("RAIL_ConfigChannels(): %d", status);
	}
	status = RAIL_SetPtiProtocol(rail_handle, RAIL_PTI_PROTOCOL_CUSTOM);
	if (status) {
		LOG_ERR("RAIL_SetPtiProtocol(): %d", status);
	}
	status = RAIL_ConfigCal(rail_handle, RAIL_CAL_TEMP | RAIL_CAL_ONETIME);
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
	status = RAIL_SetTxTransitions(rail_handle, &transitions);
	if (status) {
		LOG_ERR("RAIL_SetTxTransitions(): %d", status);
	}
	status = RAIL_SetRxTransitions(rail_handle, &transitions);
	if (status) {
		LOG_ERR("RAIL_SetRxTransitions(): %d", status);
	}
	ret = RAIL_SetTxFifo(rail_handle, tx_fifo, 0, sizeof(tx_fifo));
	if (ret != sizeof(tx_fifo)) {
		LOG_ERR("RAIL_SetTxFifo(): %d != %d", ret, sizeof(tx_fifo));
	}

	return 0;
}

static void efr32_radio_isr(const struct device* dev)
{
	LOG_INF("in radio isr");
}

#define EFR32_RADIO_IRQ_HANDLER_DECL(index)				\
	static void efr32_radio_irq_config_func_##index(const struct device *dev);
#define EFR32_RADIO_IRQ_HANDLER(index)					\
	static void efr32_radio_irq_config_func_##index(const struct device* dev)	\
	{													\
		IRQ_CONNECT(DT_INST_IRQN(index),				\
			DT_INST_IRQ(index, priority),					\
			efr32_radio_isr, DEVICE_DT_INST_GET(index), 	\
			0);												\
		irq_enable(DT_INST_IRQN(index));				\
	}

#define EFR32_RADIO_IRQ_HANDLER_FUNC(index)				\
	.irq_config_func = efr32_radio_irq_config_func_##index,

#define EFR32_RADIO_DEFINE(inst)                                   \
	EFR32_RADIO_IRQ_HANDLER_DECL(inst)							   \
    static const struct radio_conf radio_conf_##inst = { 		   \
		EFR32_RADIO_IRQ_HANDLER_FUNC(inst)						   \
    };															   \
	                                                               \
	static radio_data radio_##inst##_data = {0};                   \
                                                                   \
    DEVICE_DT_INST_DEFINE(inst,                                    \
                  efr32_radio_initialization,                      \
                  NULL,                                            \
                  &radio_##inst##_data,                            \
                  &radio_conf_##inst,                         	   \
                  POST_KERNEL,                                     \
                  CONFIG_RADIO_INIT_PRIORITY,                      \
                  &efr32_radio_api);							   \
				  												   \
	EFR32_RADIO_IRQ_HANDLER(inst)

DT_INST_FOREACH_STATUS_OKAY(EFR32_RADIO_DEFINE)
