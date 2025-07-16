#include "radio_gecko.h"

#include <zephyr/logging/log.h>

#include "pa_conversions_efr32.h"

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

static RAIL_Handle_t rail_init(void)
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

	return rail_handle;
}

int start_rx(const radio_handler* radio_handler)
{
	RAIL_Status_t status = RAIL_StartRx(radio_handler->rail_handler, radio_handler->channel, NULL);
	if (status) {
		LOG_ERR("RAIL_StartRx(): %d ", status);
	}

}

// while true??????
int receive_rx(const radio_handler* radio_handler, uint8_t* rx_frame, uint32_t len)
{
	RAIL_RxPacketHandle_t handle;
	RAIL_RxPacketInfo_t info;
	RAIL_Status_t status;

	handle = RAIL_GetRxPacketInfo(radio_handler->rail_handler, RAIL_RX_PACKET_HANDLE_OLDEST_COMPLETE,
					      &info);
	if (handle == RAIL_RX_PACKET_HANDLE_INVALID) {
		return;
	}
	if (info.packetBytes < len) {
		RAIL_CopyRxPacket(rx_frame, &info);
	}
	status = RAIL_ReleaseRxPacket(radio_handler->rail_handler, handle);
	if (status) {
		LOG_ERR("RAIL_ReleaseRxPacket(): %d", status);
	}
}

int send_tx(const radio_handler* radio_handler, const uint8_t *payload, uint32_t len)
{
	RAIL_Status_t status;
	int ret;

	ret = RAIL_WriteTxFifo(radio_handler->rail_handler, payload, len, true);
	if (ret != len) {
		LOG_ERR("RAIL_WriteTxFifo(): %d", ret);
		return;
	}

	status = RAIL_StartTx(radio_handler->rail_handler, radio_handler->channel, RAIL_TX_OPTIONS_DEFAULT, NULL);
	if (status) {
		LOG_ERR("RAIL_StartTx(): %d ", status);
	}
}
