/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NRF_RPC_TR_RPMSG_H_
#define NRF_RPC_TR_RPMSG_H_

#include <zephyr.h>
#include <metal/sys.h>
#include <metal/device.h>
#include <metal/alloc.h>
#include <openamp/open_amp.h>

#include "rp_ll.h"

/**
 * @file
 * @defgroup rp_transport Remote procedures transport
 * @{
 * @brief Remote procedures transport implementation using rpmsg
 *
 * API is compatible with rp_trans API. For API documentation
 * @see nrf_rpc_tr_tmpl.h
 */

#ifdef __cplusplus
extern "C" {
#endif

struct nrf_rpc_tr_remote_ep {
	int TODO;
};

struct nrf_rpc_tr_local_ep {
	int TODO;
};

struct nrf_rpc_tr_group {
	int TODO;
};


typedef void (*nrf_rpc_tr_receive_handler)(struct nrf_rpc_tr_remote_ep *src_ep,
					   const uint8_t *buf, size_t len);

typedef uint32_t (*nrf_rpc_tr_filter)(struct nrf_rpc_tr_remote_ep *src_ep,
				      const uint8_t *buf, size_t len);


int nrf_rpc_tr_init(nrf_rpc_tr_receive_handler callback,
		    nrf_rpc_tr_filter filter);


#define nrf_rpc_tr_alloc_tx_buf(dst_ep, buf, len)                              \
	ARG_UNUSED(dst_ep);                                                    \
	u32_t _nrf_rpc_tr_buf_vla                                              \
		[(sizeof(u32_t) - 1 + (len)) / sizeof(u32_t)];                 \
	*(buf) = (u8_t *)(&_nrf_rpc_tr_buf_vla[0])

#define nrf_rpc_tr_free_tx_buf(dst_ep, buf)

#define nrf_rpc_tr_alloc_failed(buf) 0

int nrf_rpc_tr_send(struct nrf_rpc_tr_remote_ep *dst_ep, const u8_t *buf,
		    size_t len);

int nrf_rpc_tr_read(struct nrf_rpc_tr_remote_ep **src_ep, const uint8_t **buf);

void nrf_rpc_tr_release_buffer();

struct nrf_rpc_tr_remote_ep *nrf_rpc_tr_remote_reserve(struct nrf_rpc_tr_group *group);
void nrf_rpc_tr_remote_release(struct nrf_rpc_tr_remote_ep *ep);

struct nrf_rpc_tr_remote_ep *nrf_rpc_tr_default_dst_get();
void nrf_rpc_tr_default_dst_set(struct nrf_rpc_tr_remote_ep *endpoint);

struct nrf_rpc_tr_local_ep *nrf_rpc_tr_current_get();


void printbuf(const char* text, const uint8_t *packet, size_t len); // TODO: delete

#ifdef __cplusplus
}
#endif

#endif /* NRF_RPC_TR_RPMSG_H_ */
