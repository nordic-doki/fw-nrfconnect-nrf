/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RP_LL_API_H_
#define RP_LL_API_H_

#include <zephyr.h>
#include <metal/sys.h>
#include <metal/device.h>
#include <metal/alloc.h>
#include <openamp/open_amp.h>


#ifdef __cplusplus
extern "C" {
#endif


enum rp_ll_event_type {
	RP_LL_EVENT_CONNECTED,   /**< Endpoint was created on the other side and handshake was successful */
	//RP_LL_EVENT_DISCNNECTED, /**< Endpoint was deleted on the other side  */  #PROTO: UNUSED BY US
	RP_LL_EVENT_ERROR,       /**< Endpoint was not able to connect */
	RP_LL_EVENT_DATA,        /**< New packet arrived */
};

struct rp_ll_endpoint;

/** @brief Callback called from endpoint's rx thread when a event occurred.
 * 
 */
typedef void (*rp_ll_event_handler)(struct rp_ll_endpoint *endpoint,
	enum rp_ll_event_type event, const u8_t *buf, size_t length);


struct rp_ll_endpoint {
	struct rpmsg_endpoint rpmsg_ep;
	rp_ll_event_handler callback;
	u32_t flags;
};

/** @brief Initializes
 */
int rp_ll_init(void);


/** @brief Uninitializes
 */
void rp_ll_uninit(void);

/** @brief Initializes endpoint
 * 
 * @param endpoint endpoint to initialize
 * @param endpoint_number identification of the endpoint
 *	#PROTO: We need somehow identify endpoint on both sides of endpoint. It may be different kind of identification, e.g. name (string)
 * @param callback callback called from rx thread to inform about new packets or a success or a failure of connection process
 * @param user_data transparent data point for callback #PROTO: It is not very important for us, because we can use CONTAINER_OF macro
 */
int rp_ll_endpoint_init(struct rp_ll_endpoint *endpoint,
	int endpoint_number, rp_ll_event_handler callback, void *user_data);

/** @brief Uninitializes endpoint #PROTO: We probably will not use it
 */
void rp_ll_endpoint_uninit(struct rp_ll_endpoint *endpoint);

/** @brief Sends a packet via specified endpoint.
 *
 * #PROTO: We will not send empty (zero-length) packets.
 *         In our experimental implementation we use empty packets for handshake.
 */
int rp_ll_send(struct rp_ll_endpoint *endpoint, const u8_t *buf,
	size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif
