/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <errno.h>
#include <init.h>

#include <rp_ser.h>
#include <cbor.h>

#include "../../ser_common.h"

#define SERIALIZATION_BUFFER_SIZE 64

#define ENTROPY_GET_CMD_PARAM_CNT 1

struct entropy_rsp {
	u8_t *buffer;
	u16_t length;
	int err_code;
};

RP_SER_DEFINE(entropy_ser, struct k_sem, 0, 1000, 0);

static struct entropy_rsp rsp_data;

static rp_err_t rsp_error_code_handle(CborValue *it)
{
	int err;

	if (!cbor_value_is_integer(it) ||
	    cbor_value_get_int(it, &err)) {
		return RP_ERROR_INVALID_PARAM;
	}

	rsp_data.err_code = err;

	return RP_SUCCESS;
}

static rp_err_t entropy_get_rsp(CborValue *it)
{
	int err;
	size_t buf_len = rsp_data.length;

	if (!cbor_value_is_integer(it) ||
	    cbor_value_get_int(it, &err)) {
		return RP_ERROR_INTERNAL;
	}

	rsp_data.err_code = err;

	if (cbor_value_advance_fixed(it) != CborNoError) {
		return -RP_ERROR_INTERNAL;
	}

	if (!cbor_value_is_byte_string(it) ||
	    cbor_value_copy_byte_string(it, rsp_data.buffer, &buf_len, it)) {
		return RP_ERROR_INTERNAL;
	}

	if (buf_len != rsp_data.length) {
		return RP_ERROR_INTERNAL;
	}

	return RP_SUCCESS;
}

int entropy_remote_init(void)
{
	rp_err_t err;
	struct rp_ser_encoder encoder;
	CborEncoder container;
	size_t packet_size = SERIALIZATION_BUFFER_SIZE;

	rp_ser_buf_alloc(entropy_ser, encoder, packet_size);

	err = rp_ser_procedure_initialize(&encoder, &container, 0,
					  RP_SER_PACKET_TYPE_CMD,
					  SER_COMMAND_ENTROPY_INIT);
	if (err) {
		return -EINVAL;
	}

	err = rp_ser_procedure_end(&encoder);
	if (err) {
		return -EINVAL;
	}

	err = rp_ser_cmd_send(&entropy_ser, &encoder, rsp_error_code_handle);
	if (err) {
		return -EINVAL;
	}

	return rsp_data.err_code;
}

int entropy_remote_get(u8_t *buffer, u16_t length)
{
	rp_err_t err;
	struct rp_ser_encoder encoder;
	CborEncoder container;
	size_t packet_size = SERIALIZATION_BUFFER_SIZE;

	if (!buffer || length < 1) {
		return -EINVAL;
	}

	rsp_data.buffer = buffer;
	rsp_data.length = length;

	rp_ser_buf_alloc(entropy_ser, encoder, packet_size);

	err = rp_ser_procedure_initialize(&encoder, &container,
					  ENTROPY_GET_CMD_PARAM_CNT,
					  RP_SER_PACKET_TYPE_CMD,
					  SER_COMMAND_ENTROPY_GET);
	if (err) {
		return -EINVAL;
	}

	if (cbor_encode_int(&container, length) != CborNoError) {
		return -EINVAL;
	}

	err = rp_ser_procedure_end(&encoder);
	if (err) {
		return -EINVAL;
	}

	err = rp_ser_cmd_send(&entropy_ser, &encoder, entropy_get_rsp);
	if (err) {
		return -EINVAL;
	}

	return rsp_data.err_code;
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

SYS_INIT(serialization_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
