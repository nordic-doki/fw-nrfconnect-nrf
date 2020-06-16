/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <errno.h>
#include <init.h>
#include <drivers/entropy.h>

#include <nrf_rpc.h>

#include <device.h>

#include "../../ser_common.h"

NRF_RPC_GROUP_DEFINE(entropy_group, "nrf_rpc_entropy_sample", NULL, NULL);

static struct device *entropy;

static int rsp_error_code_send(int err_code)
{
	int err;
	uint8_t *packet;

	NRF_RPC_ALLOC(packet, sizeof(int));

	*(int *)&packet[0] = err_code;

	err = nrf_rpc_rsp_send(packet, sizeof(int));
	if (err) {
		return -EINVAL;
	}

	return 0;
}

static void entropy_init_handler(const uint8_t *packet, size_t packet_len, void* handler_data)
{
	nrf_rpc_decoding_done(packet);

	entropy = device_get_binding(DT_CHOSEN_ZEPHYR_ENTROPY_LABEL);
	if (!entropy) {
		rsp_error_code_send(-EINVAL);
		return;
	}

	rsp_error_code_send(0);
}

NRF_RPC_CMD_DECODER(entropy_group, entropy_init, RPC_COMMAND_ENTROPY_INIT,
		   entropy_init_handler, NULL);

static int entropy_get_rsp(int err_code, u8_t *data, size_t length)
{
	int err;
	uint8_t *packet;

	NRF_RPC_ALLOC(packet, sizeof(int) + length);

	*(int *)&packet[0] = err_code;
	memcpy(&packet[sizeof(int)], data, length);

	err = nrf_rpc_rsp_send(packet, sizeof(int) + length);
	if (err) {
		return -EINVAL;
	}

	return 0;
}

static int entropy_get_result_evt(int err_code, u8_t *data, size_t length)
{
	int err;
	uint8_t *packet;

	NRF_RPC_ALLOC(packet, sizeof(int) + length);

	*(int *)&packet[0] = err_code;
	memcpy(&packet[sizeof(int)], data, length);

	err = nrf_rpc_evt_send(&entropy_group, RPC_EVENT_ENTROPY_GET_ASYNC_RESULT, packet, sizeof(int) + length);
	if (err) {
		return -EINVAL;
	}

	return 0;
}

static void entropy_get_handler(const uint8_t *packet, size_t packet_len, void* handler_data)
{
	int err;
	uint16_t length;
	u8_t *buf;
	bool is_event = ((int)handler_data != 0);
	
	if (packet_len < sizeof(uint16_t)) {
		nrf_rpc_decoding_done(packet);
	}

	length = *(uint16_t *)&packet[0];

	nrf_rpc_decoding_done(packet);

	buf = k_malloc(length);
	if (!buf) {
		return;
	}

	err = entropy_get_entropy(entropy, buf, length);

	if (is_event) {
		entropy_get_result_evt(err, buf, length);
	} else {
		entropy_get_rsp(err, buf, length);
	}

	k_free(buf);
}

NRF_RPC_CMD_DECODER(entropy_group, entropy_get, RPC_COMMAND_ENTROPY_GET,
		   entropy_get_handler, (void*)0);
NRF_RPC_EVT_DECODER(entropy_group, entropy_get_async, RPC_EVENT_ENTROPY_GET_ASYNC,
		   entropy_get_handler, (void*)1);

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
