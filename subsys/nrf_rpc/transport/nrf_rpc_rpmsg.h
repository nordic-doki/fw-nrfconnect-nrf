/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NRF_RPC_TR_RPMSG_H_
#define NRF_RPC_TR_RPMSG_H_

#include <zephyr.h>
#include <sys/slist.h>
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

#define NRF_RPC_TR_MAX_HEADER_SIZE 2


struct nrf_rpc_tr_remote_ep {
	sys_snode_t node;
	bool used;
	uint8_t addr;
};

struct nrf_rpc_tr_local_ep {
	uint8_t addr;
	struct k_sem input_sem;
	struct k_sem done_sem;
	bool wait_for_done;
	bool buffer_owned;
	atomic_t input_length;
	const uint8_t* input_buffer;
	void* custom_data;
};


typedef void (*nrf_rpc_tr_receive_handler)(struct nrf_rpc_tr_local_ep *dst_ep,
					   struct nrf_rpc_tr_remote_ep *src_ep,
					   const uint8_t *buf, int len);

typedef uint32_t (*nrf_rpc_tr_filter)(struct nrf_rpc_tr_local_ep *dst_ep,
				      struct nrf_rpc_tr_remote_ep *src_ep,
				      const uint8_t *buf, int len);


int nrf_rpc_tr_init(nrf_rpc_tr_receive_handler callback,
		    nrf_rpc_tr_filter filter);


#define nrf_rpc_tr_alloc_tx_buf(dst_ep, buf, len)                              \
	ARG_UNUSED(dst_ep);                                                    \
	u32_t _nrf_rpc_tr_buf_vla[(sizeof(u32_t) - 1 + ((len) + NRF_RPC_TR_MAX_HEADER_SIZE)) / sizeof(u32_t)];\
	*(buf) = ((u8_t *)(&_nrf_rpc_tr_buf_vla)) + NRF_RPC_TR_MAX_HEADER_SIZE

#define nrf_rpc_tr_free_tx_buf(dst_ep, buf)

#define nrf_rpc_tr_alloc_failed(buf) 0

int nrf_rpc_tr_send(struct nrf_rpc_tr_local_ep *local_ep, struct nrf_rpc_tr_remote_ep *dst_ep, u8_t *buf,
		    size_t len);

int nrf_rpc_tr_read(struct nrf_rpc_tr_local_ep *local_ep, struct nrf_rpc_tr_remote_ep **src_ep, const uint8_t **buf);

void nrf_rpc_tr_release_buffer(struct nrf_rpc_tr_local_ep *local_ep);

struct nrf_rpc_tr_remote_ep *nrf_rpc_tr_remote_reserve(void);
void nrf_rpc_tr_remote_release(struct nrf_rpc_tr_remote_ep *ep);

struct nrf_rpc_tr_local_ep *nrf_rpc_tr_current_get();


void *nrf_rpc_thread_custom_data_get(void);
void nrf_rpc_thread_custom_data_set(void *value);


#ifdef __cplusplus
}
#endif

#endif /* NRF_RPC_TR_RPMSG_H_ */
