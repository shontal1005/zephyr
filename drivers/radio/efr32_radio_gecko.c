#define DT_DRV_COMPAT efr32_radio_gecko
#include <stdio.h>
#include "efr32_radio_gecko.h"

#include <zephyr/logging/log.h>
#include <zephyr/irq.h>

#include "xg28_rb4401c/rail_config.h"
#include "pa_conversions_efr32.h"

radio_event_callback global_callback;

// The RX buffer need to be larger then the TX buffer
uint8_t RX_buffer[CONFIG_RADIO_EFR32_RX_BUFFER_SIZE + 1] __aligned(4);

LOG_MODULE_REGISTER(driver);

static void rail_on_channel_config(RAIL_Handle_t rail_handle,
				   const RAIL_ChannelConfigEntry_t *entry)
{
	sl_rail_util_pa_on_channel_config_change(rail_handle, entry);
}

int efr32_radio_init(const struct device* radio_dev, uint32_t configuration, uint16_t channel, radio_event_callback callback)
{
	RAIL_Status_t status;

	radio_data* data = (radio_data*)radio_dev->data;
	status = RAIL_ConfigChannels(data->rail_handle, channelConfigs[channel], &rail_on_channel_config);
	if (status) {
		LOG_ERR("RAIL_ConfigChannels(): %d", status);
		return RADIO_CONFIG_CHANNEL_FAILED;
	}
	data->channel = channel;

	data->callback = callback;
	global_callback = data->callback;

	return RADIO_SUCCESS;
}

int efr32_radio_start_cw(const struct device* radio_dev)
{
	RAIL_Status_t status;

	radio_data* data = (radio_data*)radio_dev->data;
	status = RAIL_StartTxStream(data->rail_handle, data->channel, RAIL_STREAM_CARRIER_WAVE);
	if (status != RAIL_STATUS_NO_ERROR)
	{
		return RADIO_START_CW_FAILED;
	}

	return RADIO_SUCCESS;
}

int efr32_radio_stop_cw(const struct device* radio_dev)
{
	RAIL_Status_t status;

	radio_data* data = (radio_data*)radio_dev->data;
	status = RAIL_StopTxStream(data->rail_handle);
	if (status != RAIL_STATUS_NO_ERROR)
	{
		return RADIO_STOP_CW_FAILED;
	}

	return RADIO_SUCCESS;
}

int efr32_radio_send(const struct device* radio_dev, uint16_t channel, const uint8_t* payload, int len, bool clear_fifo)
{
	radio_data* data = (radio_data*)radio_dev->data;

	RAIL_Status_t status;
	int ret;

	ret = RAIL_WriteTxFifo(data->rail_handle, payload, len, clear_fifo);
	if (ret != len) {
		LOG_ERR("RAIL_WriteTxFifo(): %d", ret);
		return RADIO_WRITE_TX_FIFO_FAILED;
	}

	status = RAIL_StartTx(data->rail_handle, channel, RAIL_TX_OPTIONS_DEFAULT, NULL);
	if (status) {
		LOG_ERR("RAIL_StartTx(): %d ", status);
		return RADIO_START_TX_FAILED;
	}
	LOG_HEXDUMP_INF(payload, len, "tx data:");

	return RADIO_SUCCESS;
}

int efr32_radio_start_rx_listening(const struct device* radio_dev, uint16_t channel)
{
	radio_data* data = (radio_data*)radio_dev->data;

	RAIL_Status_t status = RAIL_StartRx(data->rail_handle, channel, NULL);
	if (status) {
		LOG_ERR("RAIL_StartRx(): %d ", status);
		return RADIO_START_RX_FAILED;
	}

	return RADIO_SUCCESS;
}

static const struct radio_api efr32_radio_api = {
	.radio_init = efr32_radio_init,
	.radio_start_cw = efr32_radio_start_cw,
	.radio_stop_cw = efr32_radio_stop_cw,
	.radio_send = efr32_radio_send,
	.radio_start_rx_listening = efr32_radio_start_rx_listening,
};

static void rail_on_rf_ready(RAIL_Handle_t rail_handle)
{
	LOG_INF("radio is ready %p", rail_handle);
}

void rx_packets(RAIL_Handle_t rail_handle)
{
	RAIL_RxPacketHandle_t handle;
	RAIL_RxPacketInfo_t info;
	RAIL_Status_t status;

	for (;;) {
		handle = RAIL_GetRxPacketInfo(rail_handle, RAIL_RX_PACKET_HANDLE_OLDEST_COMPLETE,
					      &info);
		if (handle == RAIL_RX_PACKET_HANDLE_INVALID) {
			return;
		}
		if (info.packetBytes < sizeof(RX_buffer)) {
			RAIL_CopyRxPacket(RX_buffer, &info);
		}
		status = RAIL_ReleaseRxPacket(rail_handle, handle);
		if (status) {
			LOG_ERR("RAIL_ReleaseRxPacket(): %d", status);
		}
		if (info.packetBytes < sizeof(RX_buffer)) {
			LOG_HEXDUMP_INF(RX_buffer, info.packetBytes, "rx data:");
		} else {
			LOG_INF("rx: skip large packet");
		}
	}
}

void rail_on_event(RAIL_Handle_t rail_handler, RAIL_Events_t events)
{
	RAIL_Status_t status;

	// radio_data* data = CONTAINER_OF((&rail_handler), radio_data, rail_handle);
	radio_data* data = ((uint8_t*)(rail_handler)) - offsetof(radio_data, rail_handle);
	LOG_INF( "**** rail_handler: %p, data in on_event: %p ****", &rail_handler, data);
	if (events & RAIL_EVENTS_RX_COMPLETION) {
		if (events & RAIL_EVENT_RX_PACKET_RECEIVED) {
			RAIL_HoldRxPacket(rail_handler);
			rx_packets(rail_handler);
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
		status = RAIL_Calibrate(rail_handler, NULL, RAIL_CAL_ALL_PENDING);
		if (status) {
			LOG_ERR("RAIL_Calibrate(): %d", status);
		}
	}
}

static void rail_isr_installer(void)
{
#if defined(CONFIG_SOC_SERIES_EFR32MG24) || defined(CONFIG_SOC_SERIES_EFR32ZG28)
	IRQ_CONNECT(SYNTH_IRQn, 0, SYNTH_IRQHandler, NULL, 0);
#else
	IRQ_CONNECT(RDMAILBOX_IRQn, 0, RDMAILBOX_IRQHandler, NULL, 0);
#endif
	IRQ_CONNECT(RAC_SEQ_IRQn, 0, RAC_SEQ_IRQHandler, NULL, 0);
	IRQ_CONNECT(RAC_RSM_IRQn, 0, RAC_RSM_IRQHandler, NULL, 0);
	IRQ_CONNECT(PROTIMER_IRQn, 0, PROTIMER_IRQHandler, NULL, 0);
	IRQ_CONNECT(MODEM_IRQn, 0, MODEM_IRQHandler, NULL, 0);
	IRQ_CONNECT(FRC_IRQn, 0, FRC_IRQHandler, NULL, 0);
	IRQ_CONNECT(BUFC_IRQn, 0, BUFC_IRQHandler, NULL, 0);
	IRQ_CONNECT(AGC_IRQn, 0, AGC_IRQHandler, NULL, 0);
}

static int efr32_radio_initialization(const struct device* dev)
{
	radio_data* data = (radio_data*)dev->data;

	rail_isr_installer();
	sl_rail_util_pa_init();

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

	RAIL_Status_t status;
	int ret;

	data->rail_handle = RAIL_Init(&rail_config, &rail_on_rf_ready);
	if (!data->rail_handle) {
		LOG_ERR("RAIL_Init() failed");
	}
	LOG_INF("*** rail handle in init: %p data: %p ***\n", &data->rail_handle, data);

	status = RAIL_ConfigData(data->rail_handle, &data_config);
	if (status) {
		LOG_ERR("RAIL_ConfigData(): %d", status);
	}
	status = RAIL_SetPtiProtocol(data->rail_handle, RAIL_PTI_PROTOCOL_CUSTOM);
	if (status) {
		LOG_ERR("RAIL_SetPtiProtocol(): %d", status);
	}
	status = RAIL_ConfigCal(data->rail_handle, RAIL_CAL_TEMP | RAIL_CAL_ONETIME);
	if (status) {
		LOG_ERR("RAIL_ConfigCal(): %d", status);
	}
	status = RAIL_ConfigEvents(data->rail_handle, RAIL_EVENTS_ALL,
				   RAIL_EVENTS_RX_COMPLETION |
				   RAIL_EVENTS_TX_COMPLETION |
				   RAIL_EVENTS_TXACK_COMPLETION |
				   RAIL_EVENT_CAL_NEEDED);
	if (status) {
		LOG_ERR("RAIL_ConfigEvents(): %d", status);
	}
	status = RAIL_SetRxTransitions(data->rail_handle, &transitions);
	if (status) {
		LOG_ERR("RAIL_SetRxTransitions(): %d", status);
	}
	ret = RAIL_SetTxFifo(data->rail_handle, data->TX_buffer, 0, CONFIG_RADIO_EFR32_TX_BUFFER_SIZE);
	if (ret != CONFIG_RADIO_EFR32_TX_BUFFER_SIZE) {
		LOG_ERR("RAIL_SetTxFifo(): %d != %d", ret, CONFIG_RADIO_EFR32_TX_BUFFER_SIZE);
	}
	ret = RAIL_SetFixedLength(data->rail_handle, CONFIG_RADIO_EFR32_TX_BUFFER_SIZE);
	if (ret != CONFIG_RADIO_EFR32_TX_BUFFER_SIZE) {
		LOG_ERR("RAIL_SetFixedLength(): %d ", ret);
	}
	return 0;
}

#define EFR32_RADIO_DEFINE(inst)                                   \
    static const struct radio_conf radio_conf_##inst = { 		   \
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


DT_INST_FOREACH_STATUS_OKAY(EFR32_RADIO_DEFINE)
