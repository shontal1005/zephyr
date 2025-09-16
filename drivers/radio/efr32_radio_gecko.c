#define DT_DRV_COMPAT efr32_radio_gecko

#include <stdio.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include "efr32_radio_gecko.h"
#include "xg28_rb4401c/rail_config.h"
#include "pa_conversions_efr32.h"

LOG_MODULE_REGISTER(driver);

struct radio_data* radio_driver_data = NULL;
const struct radio_api* radio_driver_api = NULL;

#define VARIABLE_LENGTH_LENGTH_FIELD (2)

#ifdef CONFIG_RADIO_EFR32
	/**
	 * From silabs rail efr32 documentation: https://docs.silabs.com/rail/latest/rail-api/efr32-main
	 * The chosen buffer size limits the maximum size of receive packets in packet mode and determines the size of the receive
	 * FIFO in FIFO mode. Because each receive packet has several bytes of overhead,
	 * you can only receive up to one (buffer size - overhead) byte packet without switching to FIFO mode.
	 * In FIFO mode, you must read out packet data as you approach this limit and store it off to construct the full packet later.
	 * This overhead is currently 8 bytes on all EFR32 Series-2 platforms. Note that this overhead may increase or decrease
	 * in future releases as the functionality is changed though large jumps are not expected in either direction.
	 *
	 * The RAIL overhead is added to each packet in RX FIFO
	 *
	 * Payload size most be smaller than FIFO_SIZE - EFR32_PACKET_STORAGE_OVERHEAD(8 bytes) - SIZE_FIELD_LENGTH(2 bytes)
	 */
	#define RADIO_EFR32_PACKET_STORAGE_OVERHEAD (8)
#else
	#define RADIO_EFR32_PACKET_STORAGE_OVERHEAD (0)
#endif

/**
 * This is the size of the actual data from the user. In the
 * driver, the payload size (2 bytes) and rail overhead (8 bytes) are added to the payload.
 */
#define RADIO_EFR32_MAX_PACKET_LENGTH (CONFIG_RADIO_EFR32_TX_BUFFER_SIZE - RADIO_EFR32_PACKET_STORAGE_OVERHEAD - VARIABLE_LENGTH_LENGTH_FIELD)

typedef enum communication_status {

	/* Status default value */
	UNINITIALIZED = -1,

	/* The packet successfully sent */
	RX_SUCCESS,

	/* packet received size larger then the RX buffer size */
	RX_PACKET_TOO_LARGE,

	/* The RX rail handler invalid */
	RX_HANDLER_INVALID,

} communication_status_t;

/**
 * Callback function that will be called after RAIL_ConfigChannels is done.
 *
 * @param[in] railHandle A RAIL instance handle.
 * @param[in] entry A pointer to channel config entry.
 *
 */
static void rail_on_channel_config(RAIL_Handle_t rail_handle,
				   const RAIL_ChannelConfigEntry_t *entry)
{
	sl_rail_util_pa_on_channel_config_change(rail_handle, entry);
}

/**
 * Radio initialization function.
 * This function init all the user configurable configurations
 *
 * @param[in] radio_dev The radio device object.
 * @param[in] configuration PHY configuration number.
 * @param[in] channel The radio channel to work with.
 * @param[in] callback Callback function to be called after the initialization is over.
 *
 * @return Status code indicates error.
 *
 */
int efr32_radio_init(const struct device* radio_dev, uint32_t configuration, uint16_t channel, radio_event_callback callback)
{
	RAIL_Status_t status;

	radio_driver_data = (struct radio_data*)radio_dev->data;
	radio_driver_api = (struct radio_api*)radio_dev->api;

	status = RAIL_ConfigChannels(radio_driver_data->rail_handle, channelConfigs[channel], &rail_on_channel_config);
	if (status) {
		LOG_ERR("RAIL_ConfigChannels(): %d", status);
		return RADIO_CONFIG_CHANNEL_FAILED;
	}

	radio_driver_data->channel = channel;
	radio_driver_data->callback = callback;

	return RADIO_SUCCESS;
}

/**
 * Send continuous wave in the channel configured in the driver
 *
 * @return Status code indicates error.
 *
 */
int efr32_radio_start_cw()
{
	RAIL_Status_t status;

	status = RAIL_StartTxStream(radio_driver_data->rail_handle, radio_driver_data->channel, RAIL_STREAM_CARRIER_WAVE);
	if (status != RAIL_STATUS_NO_ERROR)
	{
		return RADIO_START_CW_FAILED;
	}

	return RADIO_SUCCESS;
}

/**
 * Stop the continuous wave send.
 *
 * @return Status code indicates error.
 *
 */
int efr32_radio_stop_cw()
{
	RAIL_Status_t status;

	status = RAIL_StopTxStream(radio_driver_data->rail_handle);
	if (status != RAIL_STATUS_NO_ERROR)
	{
		return RADIO_STOP_CW_FAILED;
	}

	return RADIO_SUCCESS;
}

/**
 * In RAIL variable length mode, we can create packets in different
 * sizes. Variable length packet format requires the first 2 bytes to be the payload length
 * and after that the data is inserted.
 * The function get the payload and size, and create the packet buffer in the correct variable
 * length format.
 *
 * @param[in,out] payload_to_send A buffer for the variable length packet buffer.
 * @param[in] payload The data to send.
 * @param[in] len Payload size.
 *
 *
 * @return Status code indicates error.
 *
 */
int create_radio_packet(uint8_t* payload_to_send, const uint8_t* payload, uint16_t len)
{
	*(uint16_t*)payload_to_send = sys_cpu_to_le16(len);
	void* ret = memcpy(payload_to_send + VARIABLE_LENGTH_LENGTH_FIELD, payload, len);
	if (ret == NULL)
	{
		return RADIO_TX_CREATE_PACKET_FAILED;
	}

	return RADIO_SUCCESS;
}

/**
 * Transmits the given data through the given channel.
 *
 * @param[in] channel The channel to use.
 * @param[in] payload The data to send.
 * @param[in] len Payload size.
 * @param[in] clear_fifo Whether to clear the transmit FIFO before inserting new data.
 * (NOT recommended if previous data may still exist)
 *
 * @return Status code indicates error.
 *
 */
int efr32_radio_send(uint16_t channel, const uint8_t* payload, int len, bool clear_fifo)
{
	RAIL_Status_t status;
	int ret;

	if (len > RADIO_EFR32_MAX_PACKET_LENGTH || len <= 0)
	{
		LOG_ERR("radio_send() invalid payload length %d", len);
		return RADIO_TX_CREATE_PACKET_FAILED;
	}

	ret = create_radio_packet(radio_driver_data->payload_to_send, payload, len);
	if (ret != RADIO_SUCCESS)
	{
		return RADIO_TX_CREATE_PACKET_FAILED;
	}

	ret = RAIL_WriteTxFifo(radio_driver_data->rail_handle, radio_driver_data->payload_to_send, len + VARIABLE_LENGTH_LENGTH_FIELD, clear_fifo);
	if (ret != len + VARIABLE_LENGTH_LENGTH_FIELD) {
		LOG_ERR("RAIL_WriteTxFifo(): %d", ret);
		return RADIO_WRITE_TX_FIFO_FAILED;
	}

	status = RAIL_StartTx(radio_driver_data->rail_handle, channel, RAIL_TX_OPTIONS_DEFAULT, NULL);
	if (status) {
		LOG_ERR("RAIL_StartTx(): %d ", status);
		return RADIO_START_TX_FAILED;
	}
	LOG_HEXDUMP_INF(radio_driver_data->payload_to_send, len + VARIABLE_LENGTH_LENGTH_FIELD, "tx data:");

	return RADIO_SUCCESS;
}

/**
 * Start the receiver on a specific channel.
 * This is a non-blocking function.
 *
 * @param[in] channel The channel to use.
 *
 * @return Status code indicates error.
 *
 */
int efr32_radio_start_rx_listening(uint16_t channel)
{
	RAIL_Status_t status = RAIL_StartRx(radio_driver_data->rail_handle, channel, NULL);
	if (status) {
		LOG_ERR("RAIL_StartRx(): %d ", status);
		return RADIO_START_RX_FAILED;
	}

	return RADIO_SUCCESS;
}

/**
 * Get the RX packet in case RX event occured.
 *
 * @param[in] rail_handle A RAIL instance handle.
 *
 * @return Status code indicates error.
 *
 */
int rx_packets(RAIL_Handle_t rail_handle)
{
	RAIL_RxPacketHandle_t handle;
	RAIL_RxPacketInfo_t info;
	RAIL_Status_t status;

	handle = RAIL_GetRxPacketInfo(rail_handle, RAIL_RX_PACKET_HANDLE_OLDEST_COMPLETE,
						&info);
	if (handle == RAIL_RX_PACKET_HANDLE_INVALID) {
		return RX_HANDLER_INVALID;
	}

	if (info.packetBytes < sizeof(radio_driver_data->received_packet_buffer)) {
		RAIL_CopyRxPacket(radio_driver_data->received_packet_buffer, &info);
	}
	status = RAIL_ReleaseRxPacket(rail_handle, handle);
	if (status) {
		LOG_ERR("RAIL_ReleaseRxPacket(): %d", status);
	}
	if (info.packetBytes >= sizeof(radio_driver_data->received_packet_buffer)) {
		return RX_PACKET_TOO_LARGE;
	}

	return RX_SUCCESS;
}

/**
 * A callback that will be executed when a RAIL event occurs.
 * This function handles various radio states and calls the appropriate function on each event.
 *
 * @param[in] rail_handle A RAIL instance handle.
 * @param[in] events The RAIL event that occured.
 *
 * @return Status code indicates error.
 *
 */
void rail_on_event(RAIL_Handle_t rail_handler, RAIL_Events_t events)
{
	RAIL_Status_t status;
	communication_status_t communication_status = UNINITIALIZED;

	if (events & RAIL_EVENT_CAL_NEEDED) {
		status = RAIL_Calibrate(rail_handler, NULL, RAIL_CAL_ALL_PENDING);
		if (status) {
			LOG_ERR("RAIL_Calibrate(): %d", status);
		}
	}

	if ((events & RAIL_EVENTS_RX_COMPLETION) && (events & RAIL_EVENT_RX_PACKET_RECEIVED)) {
			RAIL_HoldRxPacket(rail_handler);
			communication_status = rx_packets(rail_handler);
			if (communication_status == RX_SUCCESS) {
				radio_driver_data->event.event = RX_DONE;
				radio_driver_data->event.data = radio_driver_data->received_packet_buffer + VARIABLE_LENGTH_LENGTH_FIELD;
				radio_driver_data->event.data_size = sys_le16_to_cpu(*(uint16_t*)radio_driver_data->received_packet_buffer);
				radio_driver_data->callback(&radio_driver_data->event);
			}
	} else if ((events & RAIL_EVENTS_RX_COMPLETION) && (communication_status != RX_SUCCESS)) {
		radio_driver_data->event.event = RX_ERROR;
		radio_driver_data->event.data = NULL;
		radio_driver_data->event.data_size = 0;
		radio_driver_data->callback(&radio_driver_data->event);

		LOG_ERR("radio rx error: %08llx", events);
	}

	if (events & RAIL_EVENTS_TX_COMPLETION) {
		radio_driver_data->event.data = NULL;
		radio_driver_data->event.data_size = 0;

		if (!(events & RAIL_EVENT_TX_PACKET_SENT)) {
			radio_driver_data->event.event = TX_ERROR;
			radio_driver_data->callback(&radio_driver_data->event);

			LOG_ERR("radio tx error: %08llx", events);
		} else {
			radio_driver_data->event.event = TX_DONE;
			radio_driver_data->callback(&radio_driver_data->event);
		}
	}
}

/**
 * Configure all the needed interrupts for the
 * radio to work.
 */
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

/**
 * Callback function to be called when the RF initialization
 * is over.
 *
 * @param[in] rail_handle A RAIL instance handle.
 *
 * */
static void rail_on_rf_ready(RAIL_Handle_t rail_handle)
{
	LOG_INF("radio is ready %p", rail_handle);
}

/**
 * Radio initialization function.
 * This function init all the default radio configurations.
 *
 * @param[in] radio_dev The radio device object.
 *
 * @return 0 when the funcion is over.
 *
 */
static int efr32_radio_initialization(const struct device* dev)
{
	struct radio_data* data = (struct radio_data*)dev->data;

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
	uint16_t RX_buffer_size = CONFIG_RADIO_EFR32_RX_BUFFER_SIZE;

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
	status = RAIL_SetRxFifo(data->rail_handle, &(data->RX_buffer)[0], &RX_buffer_size);
	if (status || RX_buffer_size != CONFIG_RADIO_EFR32_RX_BUFFER_SIZE) {
		LOG_ERR("RAIL_SetRxFifo(): status: %d, sizes: %d != %d", status, RX_buffer_size, CONFIG_RADIO_EFR32_RX_BUFFER_SIZE);
	}

	return 0;
}

static const struct radio_api efr32_radio_api = {
	.radio_init = efr32_radio_init,
	.radio_start_cw = efr32_radio_start_cw,
	.radio_stop_cw = efr32_radio_stop_cw,
	.radio_send = efr32_radio_send,
	.radio_start_rx_listening = efr32_radio_start_rx_listening,
};

#define EFR32_RADIO_DEFINE(inst)                                   \
    static const struct radio_conf radio_conf_##inst = { 		   \
    };															   \
	                                                               \
	static struct radio_data radio_##inst##_data = {0};            \
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
