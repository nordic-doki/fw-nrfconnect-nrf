#include <bluetooth/services/nus.h>

#include <cbor.h>
#include <rp_ser.h>

#include <init.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(serialization);

#include "serialization.h"
#include "../../ser_common.h"

#define SERIALIZATION_BUFFER_SIZE 64

#define BT_NUS_MAX_DATA_SIZE 20
#define BT_EVT_PARAM_CNT 3
#define NUS_RSP_PARAM_CNT 1

RP_SER_DEFINE(nus_ser, struct k_sem, 0, 1000, 0);

__weak void bt_ready(int err)
{
	LOG_INF("Bluetooth initialized, err %d.", err);
}

static int rsp_error_code_sent(int err_code)
{
	rp_err_t err;
	struct rp_ser_encoder encoder;
	CborEncoder container;
	size_t packet_size = SERIALIZATION_BUFFER_SIZE;

	rp_ser_buf_alloc(nus_ser, encoder, packet_size);

	err = rp_ser_procedure_initialize(&encoder, &container,
					  NUS_RSP_PARAM_CNT,
					  RP_SER_PACKET_TYPE_RSP,
					  0);
	if (err) {
		return -EINVAL;
	}

	if (cbor_encode_int(&container, err_code) != CborNoError) {
		return -EINVAL;
	}

	err = rp_ser_procedure_end(&encoder);
	if (err) {
		return -EINVAL;
	}

	return rp_ser_rsp_send(&nus_ser, &encoder);
}

static rp_err_t bt_cmd_bt_nus_init(CborValue *it)
{
	int err;

	err = bt_enable(bt_ready);
	if (err) {
		LOG_ERR("Failed to enable Bluetooth.");
	}

	return rsp_error_code_sent(err);
}

static rp_err_t bt_cmd_gatt_nus_exec(CborValue *it)
{
	int err;
	u8_t buf[BT_NUS_MAX_DATA_SIZE];
	size_t len = sizeof(buf);

	if (!cbor_value_is_byte_string(it) ||
	    cbor_value_copy_byte_string(it, buf, &len, it) != CborNoError) {
		return RP_ERROR_INTERNAL;
	}

	err = bt_gatt_nus_send(NULL, buf, len);

	return rsp_error_code_sent(err);
}

static int ble_event_send(const bt_addr_le_t *addr, u8_t evt,
			  const u8_t *data, size_t length)
{
	rp_err_t err;
	struct rp_ser_encoder encoder;
	CborEncoder container;
	size_t packet_size = SERIALIZATION_BUFFER_SIZE;

	rp_ser_buf_alloc(nus_ser, encoder, packet_size);

	err = rp_ser_procedure_initialize(&encoder, &container,
					  BT_EVT_PARAM_CNT,
					  RP_SER_PACKET_TYPE_EVENT,
					  evt);
	if (err) {
		return -EINVAL;
	}

	if (cbor_encode_simple_value(&container, addr->type) !=
	    CborNoError) {
		return -EINVAL;
	}

	if (cbor_encode_byte_string(&container,
				    addr->a.val,
				    sizeof(addr->a.val)) !=
	     CborNoError) {
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

	return rp_ser_evt_send(&nus_ser, &encoder);

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

	rp_err_t err;

	err = rp_ser_init(&nus_ser);

	__ASSERT(err == RP_SUCCESS, "RP serialization initialization failed");

	return 0;
}

RP_SER_CMD_DECODER(nus_ser, nus_init, SER_COMMAND_NUS_INIT,
		   bt_cmd_bt_nus_init);
RP_SER_CMD_DECODER(nus_ser, nus_send, SER_COMMAND_NUS_SEND,
		   bt_cmd_gatt_nus_exec);

SYS_INIT(serialization_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);