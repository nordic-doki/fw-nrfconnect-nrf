/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RP_TRANS_RPMSG_H_
#define RP_TRANS_RPMSG_H_

#include <zephyr.h>
#include <metal/sys.h>
#include <metal/device.h>
#include <metal/alloc.h>
#include <openamp/open_amp.h>

#include "rp_ll_api.h"

/**
 * @file
 * @defgroup rp_transport Remote procedures transport
 * @{
 * @brief Remote procedures serialization transport API.
 */

#ifdef __cplusplus
extern "C" {
#endif


struct rp_trans_endpoint {
	struct rp_ll_endpoint ep;
	struct k_sem sem;
	struct k_thread thread;
	k_thread_stack_t *stack;
	void *stack_buffer;
	struct k_fifo fifo;
	volatile bool running;
};

/** @brief Callback called from endpoint's rx thread when a new packet arrived.
 * 
 */
typedef void (*rp_trans_receive_handler)(struct rp_trans_endpoint *endpoint,
	const u8_t *buf, size_t length);


/** @brief Initializes RP transport layer
 * 
 * @param callback     A callback called from rx thread with newly received
 *                     packet.
 * 
 */
int rp_trans_init(rp_trans_receive_handler callback);


/** @brief Uninitializes RP transport layer
 */
void rp_trans_uninit(void);

int rp_trans_endpoint_init(struct rp_trans_endpoint *endpoint,
	int endpoint_number, size_t stack_size, int prio);

void rp_trans_endpoint_uninit(struct rp_trans_endpoint *endpoint);

/** @brief Allocates a buffer to transmit packet.
 *
 * Allocated memory must be release by exactly one of two functions:
 * rp_trans_send or rp_trans_free_tx_buf.
 *
 * @param endpoint       Endpoint where packet will be send
 * @param[out] buf       Points to start of the buffer
 * @param[in,out] length Requested buffer length on input, actual allocated
 *                       length on output which can be different than requested.
 */
#define rp_trans_alloc_tx_buf(endpoint, buf, length) \
	ARG_UNUSED(endpoint) \
	u32_t _rp_trans_buf_vla \
		[(*(length) + sizeof(u32_t) - 1) / sizeof(u32_t)]; \
	*(buf) = (u8_t*)(&_rp_trans_buf_vla[0]);

/**  @brief Free allocated transmit buffer in case it was not send.
 */
#define rp_trans_free_tx_buf(endpoint, buf)

/**  @brief Sends a packet via specified endpoint.
 *
 * Higest bit of the first byte of the packet is reserved and must be zero. It
 * is reserved for transport internal communication.
 */
int rp_trans_send(struct rp_trans_endpoint *endpoint, const u8_t *buf,
	size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif