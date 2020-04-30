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
	nrf_rpc_cmd_ctx_t ctx;

	NRF_RPC_RSP_ALLOC(ctx, packet, sizeof(int), return -ENOMEM);

	*(int *)&packet[0] = err_code;

	err = nrf_rpc_rsp_send(ctx, packet, sizeof(int));
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
	uint8_t *packet;
	nrf_rpc_cmd_ctx_t ctx;

	NRF_RPC_RSP_ALLOC(ctx, packet, sizeof(int) + length, return -ENOMEM);

	*(int *)&packet[0] = err_code;
	memcpy(&packet[sizeof(int)], data, length);

	err = nrf_rpc_rsp_send(ctx, packet, sizeof(int) + length);
	if (err) {
		return -EINVAL;
	}

	return 0;
}

static int entropy_get_result_evt(int err_code, u8_t *data, size_t length)
{
	int err;
	uint8_t *packet;
	nrf_rpc_cmd_ctx_t ctx;

	NRF_RPC_EVT_ALLOC(ctx, &entropy_group, packet, sizeof(int) + length, return -ENOMEM);

	*(int *)&packet[0] = err_code;
	memcpy(&packet[sizeof(int)], data, length);

	err = nrf_rpc_evt_send(ctx, SER_EVENT_ENTROPY_GET_ASYNC_RESULT, packet, sizeof(int) + length);
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

	if (decoder->id == SER_EVENT_ENTROPY_GET_ASYNC) {
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
