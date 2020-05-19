#include <bluetooth/services/nus.h>

#include <nrf_rpc_cbor.h>

#include <init.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(serialization);

#include "serialization.h"
#include "../../ser_common.h"

#define CBOR_BUF_SIZE 32

#define BT_NUS_MAX_DATA_SIZE 20

NRF_RPC_GROUP_DEFINE(entropy_group, NRF_RPC_USER_GROUP_ID_FIRST);

__weak void bt_ready(int err)
{
	LOG_INF("Bluetooth initialized, err %d.", err);
}

static int rsp_error_code_sent(int err_code)
{
	int err;
	CborEncoder *encoder;
	struct nrf_rpc_cbor_rsp_ctx ctx;

	NRF_RPC_CBOR_RSP_ALLOC(ctx, encoder, CBOR_BUF_SIZE, return -ENOMEM);

	if (cbor_encode_int(encoder, err_code) != CborNoError) {
		return -EINVAL;
	}

	err = nrf_rpc_cbor_rsp_send(&ctx);
	if (err) {
		return -EINVAL;
	}

	return 0;
}

static int bt_cmd_bt_nus_init(CborValue *packet, void* handler_data)
{
	int err;

	nrf_rpc_decoding_done();

	err = bt_enable(bt_ready);
	if (err) {
		LOG_ERR("Failed to enable Bluetooth.");
	}

	return rsp_error_code_sent(err);
}

static int bt_cmd_gatt_nus_exec(CborValue *packet, void* handler_data)
{
	int err;
	CborError cbor_err;
	u8_t buf[BT_NUS_MAX_DATA_SIZE];
	size_t len = sizeof(buf);

	cbor_err = cbor_value_copy_byte_string(packet, buf, &len, packet);
	if (cbor_err) {
		return NRF_RPC_ERR_INTERNAL;
	}

	nrf_rpc_decoding_done();

	err = bt_gatt_nus_send(NULL, buf, len);

	return rsp_error_code_sent(err);
}

static int ble_event_send(const bt_addr_le_t *addr, u8_t evt,
			  const u8_t *data, size_t length)
{
	int err;
	CborEncoder *encoder;
	struct nrf_rpc_cbor_evt_ctx ctx;

	NRF_RPC_CBOR_EVT_ALLOC(ctx, &entropy_group, encoder, CBOR_BUF_SIZE, return -ENOMEM);

	if (cbor_encode_simple_value(encoder, addr->type) !=
	    CborNoError) {
		return -EINVAL;
	}

	if (cbor_encode_byte_string(encoder,
				    addr->a.val,
				    sizeof(addr->a.val)) !=
	     CborNoError) {
		return -EINVAL;
	}

	if (cbor_encode_byte_string(encoder, data, length) !=
	    CborNoError) {
		return -EINVAL;
	}

	err = nrf_rpc_cbor_evt_send(&ctx, evt);
	if (err) {
		return -EINVAL;
	}

	return 0;
}

int bt_nus_connection_evt_send(const bt_addr_le_t *addr, u8_t error)
{
	if (!addr) {
		return -EINVAL;
	}

	return ble_event_send(addr, SER_EVT_CONNECTED, &error, sizeof(error));
}

int bt_nus_disconnection_evt_send(const bt_addr_le_t *addr, u8_t reason)
{
	if (!addr) {
		return -EINVAL;
	}

	return ble_event_send(addr, SER_EVT_DISCONNECTED, &reason,
			      sizeof(reason));
}

int bt_nus_received_evt_send(const bt_addr_le_t *addr, const u8_t *data,
			     size_t length)
{
	if (!addr || !data) {
		return -EINVAL;
	}

	if (length < 1) {
		return -EINVAL;
	}

	return ble_event_send(addr, SER_EVT_NUS_RECEIVED, data, length);
}

static int serialization_init(struct device *dev)
{
	ARG_UNUSED(dev);

	int err;

	printk("Init begin\n");

	err = nrf_rpc_init();
	if (err) {
		return -EINVAL;
	}

	printk("Init done\n");

	return 0;
}

NRF_RPC_CBOR_CMD_DECODER(entropy_group, nus_init, SER_COMMAND_NUS_INIT,
			 bt_cmd_bt_nus_init, NULL);
NRF_RPC_CBOR_CMD_DECODER(entropy_group, nus_send, SER_COMMAND_NUS_SEND,
			 bt_cmd_gatt_nus_exec, NULL);

SYS_INIT(serialization_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);