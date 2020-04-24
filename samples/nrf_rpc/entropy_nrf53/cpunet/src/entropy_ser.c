/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <errno.h>
#include <init.h>
#include <drivers/entropy.h>

#include <tinycbor/cbor.h>
#include <nrf_rpc_cbor.h>

#include <device.h>

#include "../../ser_common.h"


#define CBOR_BUF_SIZE 16

NRF_RPC_GROUP_DEFINE(entropy_group, NRF_RPC_USER_GROUP_ID_FIRST);

static struct device *entropy;

static int rsp_error_code_send(int err_code)
{
	int err;
	CborEncoder *encoder;
	struct nrf_rpc_cbor_alloc_ctx ctx;

	NRF_RPC_CBOR_RSP_ALLOC(ctx, encoder, CBOR_BUF_SIZE, return -ENOMEM);

	cbor_encode_int(encoder, err_code);

	err = nrf_rpc_cbor_rsp_send(&ctx);
	if (err) {
		return -EINVAL;
	}

	return 0;
}

static int entropy_init_handler(CborValue *packet, void *handler_data)
{
	int err;

	nrf_rpc_decoding_done();

	entropy = device_get_binding(DT_CHOSEN_ZEPHYR_ENTROPY_LABEL);
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

NRF_RPC_CBOR_CMD_DECODER(entropy_group, entropy_init, RPC_COMMAND_ENTROPY_INIT,
			 entropy_init_handler, NULL);

static int entropy_get_rsp(int err_code, u8_t *data, size_t length)
{
	int err;
	CborEncoder *encoder;
	struct nrf_rpc_cbor_alloc_ctx ctx;

	NRF_RPC_CBOR_RSP_ALLOC(ctx, encoder, CBOR_BUF_SIZE + length,
			       return -ENOMEM);

	cbor_encode_int(encoder, err_code);
	cbor_encode_byte_string(encoder, data, length);

	err = nrf_rpc_cbor_rsp_send(&ctx);
	if (err) {
		return -EINVAL;
	}

	return 0;
}

static int entropy_get_result_evt(int err_code, u8_t *data, size_t length)
{
	int err;
	CborEncoder *encoder;
	struct nrf_rpc_cbor_alloc_ctx ctx;

	NRF_RPC_CBOR_EVT_ALLOC(ctx, &entropy_group, encoder,
			       CBOR_BUF_SIZE + length, return -ENOMEM);

	cbor_encode_int(encoder, err_code);
	cbor_encode_byte_string(encoder, data, length);

	err = nrf_rpc_cbor_evt_send(&ctx, RPC_EVENT_ENTROPY_GET_ASYNC_RESULT);
	if (err) {
		return -EINVAL;
	}

	return 0;
}

static int entropy_get_handler(CborValue *packet, void *handler_data)
{
	CborError cbor_err;
	int err;
	int length;
	u8_t *buf;
	uintptr_t async = (uintptr_t)handler_data;

	cbor_err = cbor_value_get_int(packet, &length);

	nrf_rpc_decoding_done();

	if (cbor_err != CborNoError || length < 0 || length > 0xFFFF) {
		return NRF_RPC_ERR_INTERNAL;
	}

	buf = k_malloc(length);
	if (!buf) {
		return NRF_RPC_ERR_NO_MEM;
	}

	err = entropy_get_entropy(entropy, buf, length);

	if (async) {
		err = entropy_get_result_evt(err, buf, length);
	} else {
		err = entropy_get_rsp(err, buf, length);
	}

	k_free(buf);

	if (err) {
		return NRF_RPC_ERR_INTERNAL;
	}

	return NRF_RPC_SUCCESS;
}

NRF_RPC_CBOR_CMD_DECODER(entropy_group, entropy_get, RPC_COMMAND_ENTROPY_GET,
			 entropy_get_handler, (void *)0);
NRF_RPC_CBOR_EVT_DECODER(entropy_group, entropy_get_async,
			 RPC_EVENT_ENTROPY_GET_ASYNC, entropy_get_handler,
			 (void *)1);

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
