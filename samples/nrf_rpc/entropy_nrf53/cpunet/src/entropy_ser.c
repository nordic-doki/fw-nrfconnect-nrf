/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <errno.h>
#include <init.h>
#include <drivers/entropy.h>

#include <nrf_rpc.h>
#include <cbor.h>

#include <device.h>

#include "../../ser_common.h"

NRF_RPC_GROUP_DEFINE(entropy_group, NRF_RPC_USER_GROUP_FIRST);

static struct device *entropy;

static int rsp_error_code_send(int err_code)
{
	rp_err_t err;
	uint8_t* packet;

	NRF_RPC_RSP_ALLOC(&packet, sizeof(int), return -ENOMEM);

	*(int *)&packet[0] = err_code;

	err = NRF_RPC_RSP_SEND(packet, sizeof(int));
	if (err) {
		return -EINVAL;
	}

	return 0;
}

static rp_err_t entropy_init_handler(const uint8_t *packet, size_t packet_len, void* handler_data)
{
	int err;

	nrf_rpc_decoding_done();

	entropy = device_get_binding(CONFIG_ENTROPY_NAME);
	if (!entropy) {
		rsp_error_code_send(-EINVAL);

		return NRF_RPC_ERR_INTERNAL;
	}

	err = rsp_error_code_send(0);
	if (err) {
		return NRF_RPC_ERR_INTERNAL;
	}

	return NRF_RPC_SUCCESS;
}

NRF_RPC_CMD_DECODER(entropy_group, entropy_init, SER_COMMAND_ENTROPY_INIT,
		   entropy_init_handler, NULL);

#if 0
RP_SER_DEFINE(entropy_ser, 0, 2048, 3);

static struct device *entropy;

static int rsp_error_code_send(int err_code)
{
	rp_err_t err;

	RP_SER_RSP_ALLOC(rsp_buf, &entropy_ser, sizeof(int));

	if (RP_SER_ALLOC_FAILED(rsp_buf)) {
		return -ENOMEM;
	}

	*(int *)&rsp_buf[0] = err_code;

	err = rp_ser_rsp_send(&entropy_ser, rsp_buf, sizeof(int));
	if (err) {
		return -EINVAL;
	}

	return 0;
}

static int entropy_get_rsp(int err_code, u8_t *data, size_t length)
{
	rp_err_t err;

	RP_SER_RSP_ALLOC(rsp_buf, &entropy_ser, sizeof(int) + length);

	if (RP_SER_ALLOC_FAILED(rsp_buf)) {
		return -ENOMEM;
	}

	*(int *)&rsp_buf[0] = err_code;
	memcpy(&rsp_buf[sizeof(int)], data, length);

	err = rp_ser_rsp_send(&entropy_ser, rsp_buf, sizeof(int) + length);
	if (err) {
		return -EINVAL;
	}

	return 0;
}

static int entropy_get_result_evt(int err_code, u8_t *data, size_t length)
{
	rp_err_t err;

	RP_SER_EVT_ALLOC(rsp_buf, &entropy_ser, sizeof(int) + length);

	if (RP_SER_ALLOC_FAILED(rsp_buf)) {
		return -ENOMEM;
	}

	*(int *)&rsp_buf[0] = err_code;
	memcpy(&rsp_buf[sizeof(int)], data, length);

	err = rp_ser_evt_send(&entropy_ser, SER_EVENT_ENTROPY_GET_ASYNC_RESULT, rsp_buf, sizeof(int) + length);
	if (err) {
		return -EINVAL;
	}

	return 0;
}

static rp_err_t entropy_init_handler(uint8_t code, const uint8_t *packet, size_t packet_len)
{
	int err;

	rp_ser_handler_decoding_done(&entropy_ser);

	entropy = device_get_binding(CONFIG_ENTROPY_NAME);
	if (!entropy) {
		rsp_error_code_send(-EINVAL);

		return RP_ERROR_INTERNAL;
	}

	err = rsp_error_code_send(0);
	if (err) {
		return RP_ERROR_INTERNAL;
	}

	return RP_SUCCESS;
}

static rp_err_t entropy_get_handler(uint8_t code, const uint8_t *packet, size_t packet_len)
{
	int err;
	uint16_t length;
	u8_t *buf;
	
	if (packet_len < sizeof(uint16_t)) {
		rp_ser_handler_decoding_done(&entropy_ser);
		return RP_ERROR_INTERNAL;
	}

	length = *(uint16_t *)&packet[0];

	rp_ser_handler_decoding_done(&entropy_ser);

	buf = k_malloc(length);
	if (!buf) {
		return RP_ERROR_INTERNAL;
	}

	err = entropy_get_entropy(entropy, buf, length);

	if (code == SER_EVENT_ENTROPY_GET_ASYNC) {
		err = entropy_get_result_evt(err, buf, length);
	} else {
		err = entropy_get_rsp(err, buf, length);
	}

	if (err) {
		return RP_ERROR_INTERNAL;
	}

	k_free(buf);

	return RP_SUCCESS;
}

static int serialization_init(struct device *dev)
{
	ARG_UNUSED(dev);

	rp_err_t err;

	err = rp_ser_init(&entropy_ser);
	if (err) {
		return -EINVAL;
	}

	return 0;
}

RP_SER_CMD_DECODER(entropy_ser, entropy_init, SER_COMMAND_ENTROPY_INIT,
		   entropy_init_handler);
RP_SER_CMD_DECODER(entropy_ser, entropy_get, SER_COMMAND_ENTROPY_GET,
		   entropy_get_handler);
RP_SER_EVT_DECODER(entropy_ser, entropy_get_async, SER_EVENT_ENTROPY_GET_ASYNC,
		   entropy_get_handler);

SYS_INIT(serialization_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
#endif
