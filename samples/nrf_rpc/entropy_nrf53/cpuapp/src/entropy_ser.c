/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <errno.h>
#include <init.h>

#include <cbor.h>

#include <nrf_rpc_cbor.h>

#include "../../ser_common.h"


#define CBOR_BUF_SIZE 16


struct entropy_get_result {
	u8_t *buffer;
	size_t length;
	int result;
};


static void (*async_callback)(int result, u8_t* buffer, size_t length);


NRF_RPC_GROUP_DEFINE(entropy_group, NRF_RPC_USER_GROUP_ID_FIRST);


int rsp_error_code_handle(CborValue *parser, void *hander_data)
{
	CborError cbor_err;

	if (!cbor_value_is_integer(parser)) {
		*(int *)hander_data = -EINVAL;
		return NRF_RPC_SUCCESS;
	}

	cbor_err = cbor_value_get_int(parser, (int *)hander_data);
	if (cbor_err != CborNoError) {
		*(int *)hander_data = -EINVAL;
		return NRF_RPC_SUCCESS;
	}

	return NRF_RPC_SUCCESS;
}


int entropy_remote_init(void)
{
	int result;
	int err;
	CborEncoder *encoder;
	struct nrf_rpc_cbor_cmd_ctx ctx;

	NRF_RPC_CBOR_CMD_ALLOC(ctx, &entropy_group, encoder, CBOR_BUF_SIZE, return -ENOMEM);

	err = nrf_rpc_cbor_cmd_send(&ctx, SER_COMMAND_ENTROPY_INIT, rsp_error_code_handle, &result);
	if (err != NRF_RPC_SUCCESS) {
		return -EINVAL;
	}

	return result;
}


static int entropy_get_rsp(CborValue *parser, void *hander_data)
{
	size_t buflen;
	CborError cbor_err;
	struct entropy_get_result *result = (struct entropy_get_result *)hander_data;

 	if (!cbor_value_is_integer(parser)) {
		goto cbor_error_exit;
	}

	cbor_err = cbor_value_get_int(parser, &result->result);
	if (cbor_err != CborNoError) {
		goto cbor_error_exit;
	}

	cbor_err = cbor_value_advance(parser);
	if (cbor_err != CborNoError) {
		goto cbor_error_exit;
	}

	buflen = result->length;
	cbor_err = cbor_value_copy_byte_string(parser, result->buffer, &buflen, NULL);
	if (cbor_err != CborNoError || buflen != result->length) {
		goto cbor_error_exit;
	}
	
	return NRF_RPC_SUCCESS;

cbor_error_exit:
	result->result = -EINVAL;
	return NRF_RPC_SUCCESS;
}


int entropy_remote_get(u8_t *buffer, size_t length)
{
	int err;
	CborEncoder *encoder;
	struct nrf_rpc_cbor_cmd_ctx ctx;
	struct entropy_get_result result = {
		.buffer = buffer,
		.length = length,
	};

	if (!buffer || length < 1) {
		return -EINVAL;
	}

	NRF_RPC_CBOR_CMD_ALLOC(ctx, &entropy_group, encoder, CBOR_BUF_SIZE, return -ENOMEM);

	cbor_encode_int(encoder, length);

	err = nrf_rpc_cbor_cmd_send(&ctx, SER_COMMAND_ENTROPY_GET, entropy_get_rsp, &result);
	if (err) {
		return -EINVAL;
	}

	return result.result;
}


int entropy_remote_get_inline(u8_t *buffer, size_t length)
{
	int err;
	CborError cbor_err;
	CborEncoder *encoder;
	CborParser p;
	CborValue parser;
	struct nrf_rpc_cbor_cmd_ctx ctx;
	int result;
	size_t buflen;

	if (!buffer || length < 1) {
		return -EINVAL;
	}

	NRF_RPC_CBOR_CMD_ALLOC(ctx, &entropy_group, encoder, 16, return -ENOMEM);

	cbor_encode_int(encoder, length);

	err = nrf_rpc_cbor_cmd_rsp_send(&ctx, SER_COMMAND_ENTROPY_GET, &p, &parser);
	if (err) {
		return -EINVAL;
	}

	if (!cbor_value_is_integer(&parser)) {
		goto cbor_error_exit;
	}

	cbor_err = cbor_value_get_int(&parser, &result);
	if (cbor_err != CborNoError) {
		goto cbor_error_exit;
	}

	cbor_err = cbor_value_advance(&parser);
	if (cbor_err != CborNoError) {
		goto cbor_error_exit;
	}

	buflen = length;
	cbor_err = cbor_value_copy_byte_string(&parser, buffer, &buflen, NULL);
	if (cbor_err != CborNoError || buflen != length) {
		goto cbor_error_exit;
	}

	nrf_rpc_decoding_done();
	
	return result;

cbor_error_exit:
	nrf_rpc_decoding_done();
	return -EINVAL;
}


int entropy_remote_get_async(u16_t length, void (*callback)(int result, u8_t* buffer, size_t length))
{
	int err;
	CborEncoder *encoder;
	struct nrf_rpc_cbor_evt_ctx ctx;

	if (length < 1 || callback == NULL) {
		return -EINVAL;
	}

	async_callback = callback;

	NRF_RPC_CBOR_EVT_ALLOC(ctx, &entropy_group, encoder, 16, return -ENOMEM);

	cbor_encode_int(encoder, length);

	err = nrf_rpc_cbor_evt_send(&ctx, SER_EVENT_ENTROPY_GET_ASYNC);
	if (err) {
		return -EINVAL;
	}

	return 0;
}


static int entropy_get_result_handler(CborValue *value, void *handler_data)
{
	int err_code;
	CborError err;
	size_t length;
	u8_t buf[32];

	if (async_callback == NULL) {
		nrf_rpc_decoding_done();
		return NRF_RPC_ERR_INVALID_STATE;
	}

	err = cbor_value_get_int(value, &err_code);
	if (err != CborNoError) {
		goto cbor_error_exit;
	}

	err = cbor_value_advance(value);
	if (err != CborNoError) {
		goto cbor_error_exit;
	}

	length = sizeof(buf);
	err = cbor_value_copy_byte_string(value, buf, &length, NULL);
	if (err != CborNoError) {
		goto cbor_error_exit;
	}
	
	nrf_rpc_decoding_done();

	async_callback(err_code, buf, length);

	return NRF_RPC_SUCCESS;

cbor_error_exit:
	nrf_rpc_decoding_done();
	async_callback(-EINVAL, buf, 0);
	return NRF_RPC_SUCCESS;
}


NRF_RPC_CBOR_EVT_DECODER(entropy_group, entropy_get_result, SER_EVENT_ENTROPY_GET_ASYNC_RESULT,
		   entropy_get_result_handler, NULL);

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
