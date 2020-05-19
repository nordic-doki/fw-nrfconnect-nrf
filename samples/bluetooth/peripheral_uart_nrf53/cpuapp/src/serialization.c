#include <cbor.h>
#include <errno.h>
#include <init.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(serialization);

#include <nrf_rpc_cbor.h>

#include "serialization.h"
#include "../../ser_common.h"

#define CBOR_BUF_SIZE 32

#define BT_ADDR_SIZE 6
#define BT_NUS_MAX_DATA_SIZE 20

NRF_RPC_GROUP_DEFINE(entropy_group, NRF_RPC_USER_GROUP_ID_FIRST);

static const struct bt_nus_cb *bt_cb;

int rsp_error_code_handle(CborValue *parser, void *hander_data)
{
	CborError err;

	if (!cbor_value_is_integer(parser)) {
		*(int *)hander_data = -EINVAL;
		return NRF_RPC_SUCCESS;
	}

	err = cbor_value_get_int(parser, (int *)hander_data);
	if (err != CborNoError) {
		*(int *)hander_data = -EINVAL;
		return NRF_RPC_SUCCESS;
	}

	return NRF_RPC_SUCCESS;
}

static void connected_evt(const bt_addr_le_t *addr, u8_t err)
{
	if (bt_cb->bt_connected) {
		bt_cb->bt_connected(addr, err);
	}
}

static void disconnected_evt(const bt_addr_le_t *addr, u8_t err)
{
	if (bt_cb->bt_disconnected) {
		bt_cb->bt_disconnected(addr, err);
	}
}

static void bt_received_evt(const bt_addr_le_t *addr, const u8_t *data,
			    size_t length)
{
	if (bt_cb->bt_received) {
		bt_cb->bt_received(addr, data, length);
	}
}

static int ble_evt(CborValue *value, void *handler_data)
{
	CborError err;
        u8_t data[BT_NUS_MAX_DATA_SIZE];
	bt_addr_le_t addr;
	size_t data_len = sizeof(data);
	size_t addr_len = BT_ADDR_SIZE;
	int evt = (int)handler_data;

	err = cbor_value_get_simple_type(value, &addr.type);
	if (err != CborNoError) {
		goto cbor_error_exit;
	}

	err = cbor_value_advance_fixed(value);
	if (err != CborNoError) {
		goto cbor_error_exit;
	}

	err = cbor_value_copy_byte_string(value, addr.a.val,
					  &addr_len, value);
	if (err != CborNoError) {
		goto cbor_error_exit;
	}

	err = cbor_value_copy_byte_string(value, data,
					  &data_len, value);
	if (err != CborNoError) {
		goto cbor_error_exit;
	}

	LOG_DBG("Event: 0x%02x", evt);

	switch (evt) {
	case SER_EVT_CONNECTED:
		if (data_len != 1) {
			goto cbor_error_exit;
		}

		connected_evt(&addr, data[0]);

		break;

	case SER_EVT_DISCONNECTED:
		if (data_len != 1) {
			goto cbor_error_exit;
		}

		disconnected_evt(&addr, data[0]);

		break;

	case SER_EVT_NUS_RECEIVED:
		if (data_len < 1) {
			goto cbor_error_exit;
		}

		bt_received_evt(&addr, data, data_len);

		break;

	default:
		nrf_rpc_decoding_done();
		return NRF_RPC_ERR_NOT_SUPPORTED;
	}

	nrf_rpc_decoding_done();
	return NRF_RPC_SUCCESS;

cbor_error_exit:
	nrf_rpc_decoding_done();
	return NRF_RPC_ERR_OS_ERROR;
}

void bt_nus_callback_register(const struct bt_nus_cb *cb)
{
	if (!cb) {
		return;
	}

	bt_cb = cb;
}

int bt_nus_init(void)
{
	int result;
	int err;
	CborEncoder *encoder;
	struct nrf_rpc_cbor_cmd_ctx ctx;

	NRF_RPC_CBOR_CMD_ALLOC(ctx, &entropy_group, encoder, CBOR_BUF_SIZE, return -ENOMEM);

	err = nrf_rpc_cbor_cmd_send(&ctx, SER_COMMAND_NUS_INIT, rsp_error_code_handle, &result);
	if (err != NRF_RPC_SUCCESS) {
		return -EINVAL;
	}

	return result;
}

int bt_nus_transmit(const u8_t *data, size_t length)
{
	int result;
	int err;
	CborEncoder *encoder;
	struct nrf_rpc_cbor_cmd_ctx ctx;;

	if (!data || length < 1) {
		return -EINVAL;
	}

	NRF_RPC_CBOR_CMD_ALLOC(ctx, &entropy_group, encoder, CBOR_BUF_SIZE, return -ENOMEM);

	if (cbor_encode_byte_string(encoder, data, length) !=
	    CborNoError) {
		return -EINVAL;
	}

	err = nrf_rpc_cbor_cmd_send(&ctx, SER_COMMAND_NUS_SEND, rsp_error_code_handle, &result);
	if (err != NRF_RPC_SUCCESS) {
		return -EINVAL;
	}

	return result;
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

NRF_RPC_CBOR_EVT_DECODER(entropy_group, connected, SER_EVT_CONNECTED, ble_evt, (void *)SER_EVT_CONNECTED);
NRF_RPC_CBOR_EVT_DECODER(entropy_group, disconnected, SER_EVT_DISCONNECTED, ble_evt, (void *)SER_EVT_DISCONNECTED);
NRF_RPC_CBOR_EVT_DECODER(entropy_group, nus_received, SER_EVT_NUS_RECEIVED, ble_evt, (void *)SER_EVT_NUS_RECEIVED);

SYS_INIT(serialization_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);