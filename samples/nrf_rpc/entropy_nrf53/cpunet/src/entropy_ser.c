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

NRF_RPC_GROUP_DEFINE(entropy_group, NRF_RPC_USER_GROUP_ID_FIRST);

static struct device *entropy;

static int rsp_error_code_send(int err_code)
{
	int err;
	uint8_t *packet;
	struct nrf_rpc_remote_ep *ep;

	NRF_RPC_RSP_ALLOC(ep, packet, sizeof(int), return -ENOMEM);

	*(int *)&packet[0] = err_code;

	err = nrf_rpc_rsp_send(ep, packet, sizeof(int));
	if (err) {
		return -EINVAL;
	}

	return 0;
}

static int entropy_init_handler(const uint8_t *packet, size_t packet_len, void* handler_data)
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
		   entropy_init_handler);

static int entropy_get_rsp(int err_code, u8_t *data, size_t length)
{
	int err;
	uint8_t* packet;
	struct nrf_rpc_remote_ep *ep;

	NRF_RPC_RSP_ALLOC(ep, packet, sizeof(int) + length, return -ENOMEM);

	*(int *)&packet[0] = err_code;
	memcpy(&packet[sizeof(int)], data, length);

	err = nrf_rpc_rsp_send(ep, packet, sizeof(int) + length);
	if (err) {
		return -EINVAL;
	}

	return 0;
}

static int entropy_get_result_evt(int err_code, u8_t *data, size_t length)
{
	int err;
	uint8_t* packet;
	struct nrf_rpc_remote_ep *ep;

	NRF_RPC_EVT_ALLOC(ep, &entropy_group, packet, sizeof(int) + length, return -ENOMEM);

	*(int *)&packet[0] = err_code;
	memcpy(&packet[sizeof(int)], data, length);

	err = nrf_rpc_evt_send(ep, SER_EVENT_ENTROPY_GET_ASYNC_RESULT, packet, sizeof(int) + length);
	if (err) {
		return -EINVAL;
	}

	return 0;
}

static int entropy_get_handler(const uint8_t *packet, size_t packet_len, void* handler_data)
{
	int err;
	uint16_t length;
	u8_t *buf;
	struct nrf_rpc_decoder *decoder = (struct nrf_rpc_decoder *)handler_data;
	
	if (packet_len < sizeof(uint16_t)) {
		nrf_rpc_decoding_done();
		return NRF_RPC_ERR_INTERNAL;
	}

	length = *(uint16_t *)&packet[0];

	nrf_rpc_decoding_done();

	buf = k_malloc(length);
	if (!buf) {
		return NRF_RPC_ERR_INTERNAL;
	}

	err = entropy_get_entropy(entropy, buf, length);

	if (decoder->code == SER_EVENT_ENTROPY_GET_ASYNC) {
		printk("SER_EVENT_ENTROPY_GET_ASYNC\n");
		err = entropy_get_result_evt(err, buf, length);
	} else {
		err = entropy_get_rsp(err, buf, length);
	}

	if (err) {
		return NRF_RPC_ERR_INTERNAL;
	}

	k_free(buf);

	return NRF_RPC_SUCCESS;
}

NRF_RPC_CMD_DECODER(entropy_group, entropy_get, SER_COMMAND_ENTROPY_GET,
		   entropy_get_handler);
NRF_RPC_EVT_DECODER(entropy_group, entropy_get_async, SER_EVENT_ENTROPY_GET_ASYNC,
		   entropy_get_handler);

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


SYS_INIT(serialization_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);

#if 0
RP_SER_DEFINE(entropy_ser, 0, 2048, 3);

static struct device *entropy;

static int rsp_error_code_send(int err_code)
{
	int err;

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
	int err;

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
	int err;

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

static int entropy_init_handler(uint8_t code, const uint8_t *packet, size_t packet_len)
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

static int entropy_get_handler(uint8_t code, const uint8_t *packet, size_t packet_len)
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

	int err;

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
