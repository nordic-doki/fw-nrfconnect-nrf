/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
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
 * @defgroup nrf_rpc_tr_rpmsg nRF PRC transport using RPMsg
 * @{
 * @brief nRF PRC transport implementation using RPMsg
 *
 * API is compatible with nrf_rpc_tr API. For API documentation
 * @see nrf_rpc_tr_tmpl.h
 */

#ifdef __cplusplus
extern "C" {
#endif


#define NRF_RPC_TR_MAX_HEADER_SIZE 0


typedef void (*nrf_rpc_tr_receive_handler)(const uint8_t *packet, int len);


int nrf_rpc_tr_init(nrf_rpc_tr_receive_handler callback);

#define nrf_rpc_tr_alloc_tx_buf(buf, len)				       \
	u32_t _nrf_rpc_tr_buf_vla[(sizeof(u32_t) - 1 + (len)) / sizeof(u32_t)];\
	*(buf) = (u8_t *)(&_nrf_rpc_tr_buf_vla)

#define nrf_rpc_tr_free_tx_buf(buf)

#define nrf_rpc_tr_alloc_failed(buf) 0

int nrf_rpc_tr_send(u8_t *buf, size_t len);


#ifdef __cplusplus
}
#endif

/**
 *@}
 */

#endif /* NRF_RPC_TR_RPMSG_H_ */
