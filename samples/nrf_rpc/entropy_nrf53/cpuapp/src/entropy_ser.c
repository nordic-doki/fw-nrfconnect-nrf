/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <errno.h>
#include <init.h>

#include <nrf_rpc.h>
#include <cbor.h>

#include "../../ser_common.h"

static void (*async_callback)(u8_t* buffer, size_t length);

NRF_RPC_GROUP_DEFINE(entropy_group, NRF_RPC_USER_GROUP_ID_FIRST);

int rsp_error_code_handle(const uint8_t *packet, size_t len,
				  void* hander_data)
{
	if (len < sizeof(int)) {
		return NRF_RPC_ERR_INVALID_PARAM;
	}
	*(int *)hander_data = *(int *)&packet[0];

	return NRF_RPC_SUCCESS;
}

int entropy_remote_init(void)
{
	int result;
	int err;
	uint8_t* packet;
	nrf_rpc_cmd_ctx_t ctx;

	NRF_RPC_CMD_ALLOC(ctx, &entropy_group, packet, 0, return -ENOMEM);

	err = nrf_rpc_cmd_send(ctx, SER_COMMAND_ENTROPY_INIT, packet, 0, rsp_error_code_handle, &result);
	if (err) {
		return -EINVAL;
	}

	return result;
}

struct entropy_get_result {
	u8_t *buffer;
	u16_t length;
	int result;
};

static int entropy_get_rsp(const uint8_t *packet, size_t len,
				  void* hander_data)
{
	struct entropy_get_result *result = (struct entropy_get_result *)hander_data;

	if (len != sizeof(int) + result->length) {
		return NRF_RPC_ERR_INTERNAL;
	}
	result->result = *(int *)&packet[0];

	memcpy(result->buffer, &packet[sizeof(int)], result->length);

	return NRF_RPC_SUCCESS;
}

int entropy_remote_get(u8_t *buffer, u16_t length)
{
	int err;
	uint8_t* packet;
	nrf_rpc_cmd_ctx_t ctx;
	struct entropy_get_result result = {
		.buffer = buffer,
		.length = length,
	};

	if (!buffer || length < 1) {
		return -EINVAL;
	}

	NRF_RPC_CMD_ALLOC(ctx, &entropy_group, packet, sizeof(uint16_t), return -ENOMEM);

	*(uint16_t *)&packet[0] = length;

	err = nrf_rpc_cmd_send(ctx, SER_COMMAND_ENTROPY_GET, packet, sizeof(uint16_t), entropy_get_rsp, &result);
	if (err) {
		return -EINVAL;
	}

	return result.result;
}

int entropy_remote_get_inline(u8_t *buffer, u16_t length)
{
	int err;
	uint8_t* packet;
	nrf_rpc_cmd_ctx_t ctx;
	const uint8_t *rsp;
	size_t rsp_len;
	int result;

	if (!buffer || length < 1) {
		return -EINVAL;
	}

	NRF_RPC_CMD_ALLOC(ctx, &entropy_group, packet, sizeof(uint16_t), return -ENOMEM);

	*(uint16_t *)&packet[0] = length;

	err = nrf_rpc_cmd_rsp_send(ctx, SER_COMMAND_ENTROPY_GET, packet, sizeof(uint16_t), &rsp, &rsp_len);
	if (err) {
		return -EINVAL;
	}

	if (rsp_len != sizeof(int) + length) {
		return NRF_RPC_ERR_INTERNAL;
	}
	result = *(int *)&rsp[0];

	memcpy(buffer, &rsp[sizeof(int)], length);

	nrf_rpc_decoding_done();

	return result;
}

int entropy_remote_get_async(u16_t length, void (*callback)(u8_t* buffer, size_t length))
{
	int err;
	uint8_t* packet;
	struct nrf_rpc_tr_remote_ep *ep;

	if (length < 1 || callback == NULL) {
		return -EINVAL;
	}

	NRF_RPC_EVT_ALLOC(ep, &entropy_group, packet, sizeof(uint16_t), return -ENOMEM);

	*(uint16_t *)&packet[0] = length;

	async_callback = callback;

	err = nrf_rpc_evt_send(ep, SER_EVENT_ENTROPY_GET_ASYNC, packet, sizeof(uint16_t));
	if (err) {
		return -EINVAL;
	}

	return 0;
}

static int entropy_get_result_handler(const uint8_t *packet, size_t packet_len, void* handler_data)
{
	int err;
	size_t length;
	u8_t buf[32];

	if (packet_len < sizeof(int)) {
		nrf_rpc_decoding_done();
		return NRF_RPC_ERR_INTERNAL;
	}
	err = *(int *)&packet[0];
	length = packet_len - sizeof(int);
	if (length > sizeof(buf)) {
		return NRF_RPC_ERR_NO_MEM;
	}

	memcpy(buf, &packet[sizeof(int)], length);

	nrf_rpc_decoding_done();

	if (async_callback != NULL) {
		async_callback(buf, length);
	}

	return NRF_RPC_SUCCESS;
}

NRF_RPC_EVT_DECODER(entropy_group, entropy_get_result, SER_EVENT_ENTROPY_GET_ASYNC_RESULT,
		   entropy_get_result_handler);

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
