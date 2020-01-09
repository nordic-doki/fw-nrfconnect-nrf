#include <cbor.h>
#include <errno.h>
#include <init.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(serialization);

#include <rp_ser.h>

#include "serialization.h"
#include "../../ser_common.h"

#define SERIALIZATION_BUFFER_SIZE 64

#define BT_ADDR_SIZE 6

#define BT_NUS_MAX_DATA_SIZE 20
#define NUS_SEND_CMD_PARAM_CNT 1

RP_SER_DEFINE(nus_ser, struct k_sem, 0, 1000, 0);

static const struct bt_nus_cb *bt_cb;
static int rsp_error;

static rp_err_t rsp_error_code_handle(CborValue *it)
{
	int err;

	if (!cbor_value_is_integer(it) ||
	    cbor_value_get_int(it, &err)) {
		    return RP_ERROR_INVALID_PARAM;
	}

	rsp_error = err;

	return RP_SUCCESS;
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

static rp_err_t ble_evt(uint8_t evt, CborValue *it)
{
        u8_t data[BT_NUS_MAX_DATA_SIZE];
	bt_addr_le_t addr;
	size_t data_len = sizeof(data);
	size_t addr_len = BT_ADDR_SIZE;

        if (!cbor_value_is_simple_type(it) ||
		cbor_value_get_simple_type(it, &addr.type) != CborNoError) {
		return RP_ERROR_INTERNAL;
	}

	if (cbor_value_advance_fixed(it) != CborNoError) {
		return RP_ERROR_INTERNAL;
	}

	if (!cbor_value_is_byte_string(it) ||
	    cbor_value_copy_byte_string(it, addr.a.val,
		    &addr_len, it) != CborNoError) {
		return RP_ERROR_INTERNAL;
	}

	if (!cbor_value_is_byte_string(it) ||
	    cbor_value_copy_byte_string(it, data,
		    &data_len, it) != CborNoError) {
		return RP_ERROR_INTERNAL;
	}

	LOG_DBG("Event: 0x%02x", evt);

	switch (evt) {
	case SER_EVT_CONNECTED:
		if (data_len != 1) {
			return RP_ERROR_INTERNAL;
		}

		connected_evt(&addr, data[0]);

		break;

	case SER_EVT_DISCONNECTED:
		if (data_len != 1) {
			return RP_ERROR_INTERNAL;
		}

		disconnected_evt(&addr, data[0]);

		break;

	case SER_EVT_NUS_RECEIVED:
		if (data_len < 1) {
			return RP_ERROR_INTERNAL;
		}

		bt_received_evt(&addr, data, data_len);

		break;

	default:
		return RP_ERROR_NOT_SUPPORTED;
	}

	return 0;
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
	rp_err_t err;
	struct rp_ser_encoder encoder;
	CborEncoder container;
	size_t packet_size = SERIALIZATION_BUFFER_SIZE;

	rp_ser_buf_alloc(nus_ser, encoder, packet_size);

	err = rp_ser_procedure_initialize(&encoder, &container, 0,
					  RP_SER_PACKET_TYPE_CMD,
					  SER_COMMAND_NUS_INIT);
	if (err) {
		return -EINVAL;
	}

	err = rp_ser_procedure_end(&encoder);
	if (err) {
		return-EINVAL;
	}

	err = rp_ser_cmd_send(&nus_ser, &encoder, rsp_error_code_handle);
	if (err) {
		return -EINVAL;
	}

	return rsp_error;
}

int bt_nus_transmit(const u8_t *data, size_t length)
{
	rp_err_t err;
	struct rp_ser_encoder encoder;
	CborEncoder container;
	size_t packet_size = SERIALIZATION_BUFFER_SIZE;

	if (!data || length < 1) {
		return -EINVAL;
	}

	rp_ser_buf_alloc(nus_ser, encoder, packet_size);

	err = rp_ser_procedure_initialize(&encoder, &container,
					  NUS_SEND_CMD_PARAM_CNT,
					  RP_SER_PACKET_TYPE_CMD,
					  SER_COMMAND_NUS_SEND);
	if (err) {
		return -EINVAL;
	}

	if (cbor_encode_byte_string(&container, data, length) !=
				    CborNoError) {
		return -EINVAL;
	}

	err = rp_ser_procedure_end(&encoder);
	if (err) {
		return -EINVAL;
	}

	err = rp_ser_cmd_send(&nus_ser, &encoder, rsp_error_code_handle);
	if (err) {
		return -EINVAL;
	}

	return rsp_error;
}

static int serialization_init(struct device *dev)
{
	ARG_UNUSED(dev);

	rp_err_t err;

	err = rp_ser_init(&nus_ser);

	__ASSERT(err == RP_SUCCESS, "RP serialization initialization failed");

	return 0;
}

RP_SER_EVT_DECODER(nus_ser, connected, SER_EVT_CONNECTED, ble_evt);
RP_SER_EVT_DECODER(nus_ser, disconnected, SER_EVT_DISCONNECTED, ble_evt);
RP_SER_EVT_DECODER(nus_ser, nus_received, SER_EVT_NUS_RECEIVED, ble_evt);

SYS_INIT(serialization_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);