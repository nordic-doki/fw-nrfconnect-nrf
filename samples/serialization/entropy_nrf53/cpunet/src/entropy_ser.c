/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <errno.h>
#include <init.h>
#include <drivers/entropy.h>

#include <rp_ser.h>
#include <cbor.h>

#include <device.h>

#include "../../ser_common.h"

#define SERIALIZATION_BUFFER_SIZE 64

#define ENTROPY_INIT_RSP_PARAM_CNT 1
#define ENTROPY_GET_RSP_PARAM_CNT 2

RP_SER_DEFINE(entropy_ser, struct k_sem, 0, 1000, 0);

static struct device *entropy;

static int rsp_error_code_sent(int err_code)
{
	rp_err_t err;
	struct rp_ser_encoder encoder;
	CborEncoder container;
	size_t packet_size = SERIALIZATION_BUFFER_SIZE;

	rp_ser_buf_alloc(entropy_ser, encoder, packet_size);

	err = rp_ser_procedure_initialize(&encoder, &container,
					  ENTROPY_INIT_RSP_PARAM_CNT,
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

	err = rp_ser_rsp_send(&entropy_ser, &encoder);
	if (err) {
		return -EINVAL;
	}

	return 0;
}

static int entropy_get_rsp(int err_code, u8_t *data, size_t length)
{
	rp_err_t err;
	struct rp_ser_encoder encoder;
	CborEncoder container;
	size_t packet_size = SERIALIZATION_BUFFER_SIZE;

	rp_ser_buf_alloc(entropy_ser, encoder, packet_size);

	err = rp_ser_procedure_initialize(&encoder, &container,
					  ENTROPY_GET_RSP_PARAM_CNT,
					  RP_SER_PACKET_TYPE_RSP,
					  0);
	if (err) {
		return -EINVAL;
	}

	if (cbor_encode_int(&container, err_code) != CborNoError) {
		return -EINVAL;
	}

	if (cbor_encode_byte_string(&container, data, length) != CborNoError) {
		return -EINVAL;
	}

	err = rp_ser_procedure_end(&encoder);
	if (err) {
		return -EINVAL;
	}

	err = rp_ser_rsp_send(&entropy_ser, &encoder);
	if (err) {
		return -EINVAL;
	}

	return 0;
}


static rp_err_t entropy_init_handler(CborValue *it)
{
	int err;

	entropy = device_get_binding(CONFIG_ENTROPY_NAME);
	if (!entropy) {
		rsp_error_code_sent(-EINVAL);

		return RP_ERROR_INTERNAL;
	}

	err = rsp_error_code_sent(0);
	if (err) {
		return RP_ERROR_INTERNAL;
	}

	return RP_SUCCESS;
}

static rp_err_t entropy_get_handler(CborValue *it)
{
	int err;
	int length;
	u8_t *buf;

	if (!cbor_value_is_integer(it) ||
	    cbor_value_get_int(it, &length) != CborNoError) {
		return RP_ERROR_INTERNAL;
	}

	buf = k_malloc(length);
	if (!buf) {
		return RP_ERROR_INTERNAL;
	}

	err = entropy_get_entropy(entropy, buf, length);

	err = entropy_get_rsp(err, buf, length);
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

SYS_INIT(serialization_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
