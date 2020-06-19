/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <errno.h>
#include <init.h>

#include <tinycbor/cbor.h>

#include <nrf_rpc_cbor.h>

#include "../../common_ids.h"


#define CBOR_BUF_SIZE 16


struct entropy_get_result {
	u8_t *buffer;
	size_t length;
	int result;
};


static void (*async_callback)(int result, u8_t *buffer, size_t length);


NRF_RPC_GROUP_DEFINE(entropy_group, "nrf_sample_entropy", NULL, NULL, NULL);


void rsp_error_code_handle(CborValue *value, void *handler_data)
{
	CborError cbor_err;

	if (!cbor_value_is_integer(value)) {
		*(int *)handler_data = -EINVAL;
	}

	cbor_err = cbor_value_get_int(value, (int *)handler_data);
	if (cbor_err != CborNoError) {
		*(int *)handler_data = -EINVAL;
	}
}


int entropy_remote_init(void)
{
	int result;
	int err;
	struct nrf_rpc_cbor_ctx ctx;

	NRF_RPC_CBOR_ALLOC(ctx, CBOR_BUF_SIZE);

	err = nrf_rpc_cbor_cmd(&entropy_group, RPC_COMMAND_ENTROPY_INIT, &ctx,
			       rsp_error_code_handle, &result);
	if (err < 0) {
		return err;
	}

	return result;
}


static void entropy_get_rsp(CborValue *value, void *handler_data)
{
	size_t buflen;
	CborError cbor_err;
	struct entropy_get_result *result =
		(struct entropy_get_result *)handler_data;

	if (!cbor_value_is_integer(value)) {
		goto cbor_error_exit;
	}

	cbor_err = cbor_value_get_int(value, &result->result);
	if (cbor_err != CborNoError) {
		goto cbor_error_exit;
	}

	cbor_err = cbor_value_advance(value);
	if (cbor_err != CborNoError) {
		goto cbor_error_exit;
	}

	buflen = result->length;
	cbor_err = cbor_value_copy_byte_string(value, result->buffer, &buflen,
					       NULL);
	if (cbor_err != CborNoError || buflen != result->length) {
		goto cbor_error_exit;
	}

	return;

cbor_error_exit:
	result->result = -EINVAL;
}


int entropy_remote_get(u8_t *buffer, size_t length)
{
	int err;
	struct nrf_rpc_cbor_ctx ctx;
	struct entropy_get_result result = {
		.buffer = buffer,
		.length = length,
	};

	if (!buffer || length < 1) {
		return -EINVAL;
	}

	NRF_RPC_CBOR_ALLOC(ctx, CBOR_BUF_SIZE);

	cbor_encode_int(&ctx.encoder, length);

	err = nrf_rpc_cbor_cmd(&entropy_group, RPC_COMMAND_ENTROPY_GET, &ctx,
			       entropy_get_rsp, &result);
	if (err) {
		return -EINVAL;
	}

	return result.result;
}


int entropy_remote_get_inline(u8_t *buffer, size_t length)
{
	int err;
	CborError cbor_err;
	struct nrf_rpc_cbor_rsp_ctx ctx;
	int result;
	size_t buflen;

	if (!buffer || length < 1) {
		return -EINVAL;
	}

	NRF_RPC_CBOR_ALLOC(ctx, CBOR_BUF_SIZE);

	cbor_encode_int(&ctx.encoder, length);

	err = nrf_rpc_cbor_cmd_rsp(&entropy_group, RPC_COMMAND_ENTROPY_GET,
				   &ctx);
	if (err) {
		return -EINVAL;
	}

	if (!cbor_value_is_integer(&ctx.value)) {
		goto cbor_error_exit;
	}

	cbor_err = cbor_value_get_int(&ctx.value, &result);
	if (cbor_err != CborNoError) {
		goto cbor_error_exit;
	}

	cbor_err = cbor_value_advance(&ctx.value);
	if (cbor_err != CborNoError) {
		goto cbor_error_exit;
	}

	buflen = length;
	cbor_err = cbor_value_copy_byte_string(&ctx.value, buffer, &buflen,
					       NULL);
	if (cbor_err != CborNoError || buflen != length) {
		goto cbor_error_exit;
	}

	nrf_rpc_cbor_decoding_done(&ctx.value);
	return result;

cbor_error_exit:
	nrf_rpc_cbor_decoding_done(&ctx.value);
	return -EINVAL;
}


int entropy_remote_get_async(u16_t length, void (*callback)(int result,
							    u8_t *buffer,
							    size_t length))
{
	int err;
	struct nrf_rpc_cbor_ctx ctx;

	if (length < 1 || callback == NULL) {
		return -EINVAL;
	}

	async_callback = callback;

	NRF_RPC_CBOR_ALLOC(ctx, CBOR_BUF_SIZE);

	cbor_encode_int(&ctx.encoder, length);

	err = nrf_rpc_cbor_evt(&entropy_group, RPC_EVENT_ENTROPY_GET_ASYNC,
			       &ctx);
	if (err) {
		return -EINVAL;
	}

	return 0;
}


static void entropy_get_result_handler(CborValue *value, void *handler_data)
{
	int err_code;
	CborError err;
	size_t length;
	u8_t buf[32];

	if (async_callback == NULL) {
		nrf_rpc_cbor_decoding_done(value);
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

	nrf_rpc_cbor_decoding_done(value);
	async_callback(err_code, buf, length);
	return;

cbor_error_exit:
	nrf_rpc_cbor_decoding_done(value);
	async_callback(-EINVAL, buf, 0);
	return;
}


NRF_RPC_CBOR_EVT_DECODER(entropy_group, entropy_get_result,
			 RPC_EVENT_ENTROPY_GET_ASYNC_RESULT,
			 entropy_get_result_handler, NULL);


static void err_handler(const struct nrf_rpc_err_report *report)
{
	printk("nRF RPC error %d ocurred. See nRF RPC logs for more details.",
	       report->code);
	k_oops();
}


static int serialization_init(struct device *dev)
{
	ARG_UNUSED(dev);

	int err;

	printk("Init begin\n");

	err = nrf_rpc_init(err_handler);
	if (err) {
		return -EINVAL;
	}

	printk("Init done\n");

	return 0;
}


SYS_INIT(serialization_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
