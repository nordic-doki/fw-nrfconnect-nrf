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

#include "../../common_ids.h"


#define CBOR_BUF_SIZE 16

NRF_RPC_GROUP_DEFINE(entropy_group, "nrf_sample_entropy", NULL, NULL, NULL);

static struct device *entropy;

static void rsp_error_code_send(int err_code)
{
	struct nrf_rpc_cbor_ctx ctx;

	NRF_RPC_CBOR_ALLOC(ctx, CBOR_BUF_SIZE);

	cbor_encode_int(&ctx.encoder, err_code);

	nrf_rpc_cbor_rsp_no_err(&ctx);
}

static void entropy_init_handler(CborValue *packet, void *handler_data)
{
	nrf_rpc_cbor_decoding_done(packet);

	entropy = device_get_binding(DT_CHOSEN_ZEPHYR_ENTROPY_LABEL);
	if (!entropy) {
		rsp_error_code_send(-EINVAL);
		return;
	}

	rsp_error_code_send(0);
}


NRF_RPC_CBOR_CMD_DECODER(entropy_group, entropy_init, RPC_COMMAND_ENTROPY_INIT,
			 entropy_init_handler, NULL);


static void entropy_get_rsp(int err_code, const u8_t *data, size_t length)
{
	struct nrf_rpc_cbor_ctx ctx;

	NRF_RPC_CBOR_ALLOC(ctx, CBOR_BUF_SIZE + length);

	cbor_encode_int(&ctx.encoder, err_code);
	cbor_encode_byte_string(&ctx.encoder, data, length);

	nrf_rpc_cbor_rsp_no_err(&ctx);
}

static void entropy_get_result_evt(int err_code, const u8_t *data,
				   size_t length)
{
	struct nrf_rpc_cbor_ctx ctx;

	NRF_RPC_CBOR_ALLOC(ctx, CBOR_BUF_SIZE + length);

	cbor_encode_int(&ctx.encoder, err_code);
	cbor_encode_byte_string(&ctx.encoder, data, length);

	nrf_rpc_cbor_evt_no_err(&entropy_group,
				RPC_EVENT_ENTROPY_GET_ASYNC_RESULT,
				&ctx);
}

static void entropy_get_handler(CborValue *packet, void *handler_data)
{
	CborError cbor_err;
	int err;
	int length;
	u8_t *buf;
	uintptr_t async = (uintptr_t)handler_data;

	cbor_err = cbor_value_get_int(packet, &length);

	nrf_rpc_cbor_decoding_done(packet);

	if (cbor_err != CborNoError || length < 0 || length > 0xFFFF) {
		err = -EBADMSG;
		goto error_exit;
	}

	buf = k_malloc(length);
	if (!buf) {
		err = -ENOMEM;
		goto error_exit;
	}

	err = entropy_get_entropy(entropy, buf, length);

	if (async) {
		entropy_get_result_evt(err, buf, length);
	} else {
		entropy_get_rsp(err, buf, length);
	}

	k_free(buf);

	return;

error_exit:
	if (async) {
		entropy_get_result_evt(err, "", 0);
	} else {
		entropy_get_rsp(err, "", 0);
	}
}

NRF_RPC_CBOR_CMD_DECODER(entropy_group, entropy_get, RPC_COMMAND_ENTROPY_GET,
			 entropy_get_handler, (void *)0);
NRF_RPC_CBOR_EVT_DECODER(entropy_group, entropy_get_async,
			 RPC_EVENT_ENTROPY_GET_ASYNC, entropy_get_handler,
			 (void *)1);


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
