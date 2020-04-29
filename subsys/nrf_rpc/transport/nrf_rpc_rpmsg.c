/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define NRF_RPC_LOG_MODULE NRF_RPC_TR
#include <nrf_rpc_log.h>

#include <zephyr.h>
#include <errno.h>

#include <nrf_rpc_errors.h>

#include "rp_ll.h"
#include "nrf_rpc.h"
#include "nrf_rpc_rpmsg.h"


/* Flag used in packet length variables to indicate that packet was filered. */
#define FLAG_FILTERED (0x80000000uL)


/* Address of a null endpoint to indicate that source or destination endpoint is
 * unknown or undefined.
 */
#define NULL_EP_ADDR 0x7F


/* Header content indexes */
#define HEADER_DST_INDEX 0
#define HEADER_SRC_INDEX 1


/* Utility macro for dumping content of the packets with limit of 32 bytes
 * to prevent overflowing the logs.
 */
#define DUMP_LIMITED_DBG(memory, len, text) do {			       \
	if ((len) > 32) {						       \
		NRF_RPC_DUMP_DBG(memory, 32, text " (truncated)");	       \
	} else {							       \
		NRF_RPC_DUMP_DBG(memory, (len), text);			       \
	}								       \
} while (0)


/* Upper level callbacks */
static nrf_rpc_tr_receive_handler receive_callback;
static nrf_rpc_tr_filter receive_filter;

/* Lower level endpoint instance */
static struct rp_ll_endpoint ll_endpoint;

/* Pool of remote endpoint instances */
static struct nrf_rpc_remote_ep remote_pool[
	CONFIG_NRF_RPC_REMOTE_THREAD_POOL_SIZE +
	CONFIG_NRF_RPC_REMOTE_EXTRA_EP_COUNT];

/* Semaphore counting number of free threads in remote pool */
K_SEM_DEFINE(remote_pool_sem, 0, CONFIG_NRF_RPC_REMOTE_THREAD_POOL_SIZE);

/* List of instances of endpoints that are associated with free thread from
 * remote thread pool
 */
static sys_slist_t remote_pool_free = SYS_SLIST_STATIC_INIT(&remote_pool_free);

/* Mutex guarding access to remote_pool_free list */
K_MUTEX_DEFINE(remote_pool_mutex);

/* Pool of local endpoint instances. If a thread is trying to receive anything
 * from nRF PRC then it gets permanently instance of a local endpoint.
 */
static struct nrf_rpc_local_ep local_endpoints[
	CONFIG_NRF_RPC_LOCAL_THREAD_POOL_SIZE +
	CONFIG_NRF_RPC_EXTRA_EP_COUNT];

/* Contains next available local endpoint instance in local_endpoints. */
static atomic_t next_free_extra_ep =
	ATOMIC_INIT(CONFIG_NRF_RPC_LOCAL_THREAD_POOL_SIZE);

/* Stacks for local thread pool. */
static K_THREAD_STACK_ARRAY_DEFINE(pool_stacks,
	CONFIG_NRF_RPC_LOCAL_THREAD_POOL_SIZE,
	CONFIG_NRF_RPC_LOCAL_THREAD_STACK_SIZE);

/* All threads from a local thread pool. */
struct k_thread pool_threads[CONFIG_NRF_RPC_LOCAL_THREAD_POOL_SIZE];


/* Translates RPMsg error code to nRF RPC error code. */
static int translate_error(int rpmsg_err)
{
	switch (rpmsg_err) {
	case RPMSG_ERR_NO_MEM:
		return NRF_RPC_ERR_NO_MEM;
	case RPMSG_ERR_NO_BUFF:
		return NRF_RPC_ERR_NO_MEM;
	case RPMSG_ERR_PARAM:
		return NRF_RPC_ERR_INVALID_PARAM;
	case RPMSG_ERR_DEV_STATE:
		return NRF_RPC_ERR_INVALID_STATE;
	case RPMSG_ERR_BUFF_SIZE:
		return NRF_RPC_ERR_NO_MEM;
	case RPMSG_ERR_INIT:
		return NRF_RPC_ERR_INTERNAL;
	case RPMSG_ERR_ADDR:
		return NRF_RPC_ERR_INTERNAL;
	default:
		if (rpmsg_err < 0) {
			return NRF_RPC_ERR_INTERNAL;
		}
		break;
	}
	return NRF_RPC_SUCCESS;
}


/* Event callback from lower level. */
static void ll_event_handler(struct rp_ll_endpoint *endpoint,
	enum rp_ll_event_type event, const u8_t *buf, size_t length)
{
	uint8_t dst_addr;
	uint8_t src_addr;
	struct nrf_rpc_tr_remote_ep *src;
	struct nrf_rpc_tr_local_ep *dst;
	uint32_t filtered;

	if (event == RP_LL_EVENT_CONNECTED) {

		/* remote_pool_sem is also used during initialization to
		 * wait of a connection before exiting nRF RPC init function.
		 */
		k_sem_give(&remote_pool_sem);
		return;

	} else if (event != RP_LL_EVENT_DATA ||
		   length < NRF_RPC_TR_MAX_HEADER_SIZE) {

		return;

	}

	dst_addr = buf[HEADER_DST_INDEX];
	src_addr = buf[HEADER_SRC_INDEX];

	if (src_addr < CONFIG_NRF_RPC_REMOTE_THREAD_POOL_SIZE +
		       CONFIG_NRF_RPC_REMOTE_EXTRA_EP_COUNT) {
		src = &remote_pool[src_addr].tr_ep;
	} else {
		src = NULL;
	}

	if (dst_addr >= CONFIG_NRF_RPC_LOCAL_THREAD_POOL_SIZE +
		        CONFIG_NRF_RPC_EXTRA_EP_COUNT) {
		/* Packet directed to null endpoint cannot be passed to specific
		 * thread, so only filter callback is possible. */
		receive_filter(NULL, src, &buf[NRF_RPC_TR_MAX_HEADER_SIZE],
			       length - NRF_RPC_TR_MAX_HEADER_SIZE);
		return;
	}

	dst = &local_endpoints[dst_addr].tr_ep;

	filtered = receive_filter(dst, src, &buf[NRF_RPC_TR_MAX_HEADER_SIZE],
				  length - NRF_RPC_TR_MAX_HEADER_SIZE);

	if (dst->wait_for_done) {
		/* Last packet was filered, but still we have to wait for a
		 * destination thread to consume information about filtered
		 * packet.
		 */
		k_sem_take(&dst->done_sem, K_FOREVER);
	}

	if (!filtered) {
		/* It is safe to modify decode_buffer, because other thread will
		 * read/write it only after k_sem_give and k_sem_take.
		 */
		dst->input_buffer = buf;
		/* Make sure that input_buffer was set before input_length */
		__DMB();
		atomic_set(&dst->input_length, (atomic_val_t)length);
		dst->wait_for_done = false;
	} else {
		atomic_set(&dst->input_length, (atomic_val_t)filtered |
			   (atomic_val_t)FLAG_FILTERED);
		dst->wait_for_done = true;
	}

	/* Inform destination endpoint about a new packet reception. */
	k_sem_give(&dst->input_sem);

	if (!dst->wait_for_done) {
		/* If a message was not filtered then wait until destination
		 * thread is done with the buffer.
		 */
		k_sem_take(&dst->done_sem, K_FOREVER);
	}
}

/* Main loop of each thread from a thread pool */
static void thread_pool_entry(void *p1, void *p2, void *p3)
{
	struct nrf_rpc_tr_local_ep *local_ep = (struct nrf_rpc_tr_local_ep *)p1;
	struct nrf_rpc_tr_remote_ep *src;
	const uint8_t *buf;
	int len;

	k_thread_custom_data_set(local_ep);

	do {
		len = nrf_rpc_tr_read(local_ep, &src, &buf);
		receive_callback(local_ep, src, buf, len);
	} while (true);
}


int nrf_rpc_tr_init(nrf_rpc_tr_receive_handler callback,
		    nrf_rpc_tr_filter filter)
{
	uint32_t i;
	int err = 0;

	receive_callback = callback;
	receive_filter = filter;

	k_mutex_lock(&remote_pool_mutex, K_FOREVER);

	err = rp_ll_init();
	if (err != 0) {
		goto error_exit;
	}
	
	err = rp_ll_endpoint_init(&ll_endpoint, 1, ll_event_handler, NULL);
	if (err != 0) {
		goto error_exit;
	}

	k_sem_take(&remote_pool_sem, K_FOREVER);

	for (i = 0; i < CONFIG_NRF_RPC_REMOTE_THREAD_POOL_SIZE +
			CONFIG_NRF_RPC_REMOTE_EXTRA_EP_COUNT; i++) {
				
		remote_pool[i].tr_ep.addr = i;
		if (i < CONFIG_NRF_RPC_REMOTE_THREAD_POOL_SIZE) {
			remote_pool[i].tr_ep.used = false;
			sys_slist_append(&remote_pool_free, &remote_pool[i].tr_ep.node);
			k_sem_give(&remote_pool_sem);
		}
	}

	for (i = 0; i < CONFIG_NRF_RPC_LOCAL_THREAD_POOL_SIZE; i++) {
		struct nrf_rpc_tr_local_ep *ep = &local_endpoints[i].tr_ep;
		ep->addr = i;
		k_sem_init(&ep->done_sem, 0, 1);
		k_sem_init(&ep->input_sem, 0, 1);
		k_thread_create(&pool_threads[i], pool_stacks[i],
			K_THREAD_STACK_SIZEOF(pool_stacks[i]),
			thread_pool_entry,
			ep, NULL, NULL,
			CONFIG_NRF_RPC_LOCAL_THREAD_PRIORITY, 0, K_NO_WAIT);
	}

error_exit:
	k_mutex_unlock(&remote_pool_mutex);
	return translate_error(err);
}


int nrf_rpc_tr_send(struct nrf_rpc_tr_local_ep *local_ep, struct nrf_rpc_tr_remote_ep *dst_ep, u8_t *buf,
		    size_t len)
{
	int err;
	u8_t *full_packet = &buf[-NRF_RPC_TR_MAX_HEADER_SIZE];

	full_packet[HEADER_DST_INDEX] = dst_ep ? dst_ep->addr : NULL_EP_ADDR;
	full_packet[HEADER_SRC_INDEX] = local_ep ? local_ep->addr : NULL_EP_ADDR;

	DUMP_LIMITED_DBG(full_packet, len + NRF_RPC_TR_MAX_HEADER_SIZE, "Send");

	err = rp_ll_send(&ll_endpoint, full_packet, len + NRF_RPC_TR_MAX_HEADER_SIZE);

	return translate_error(err);
}


int nrf_rpc_tr_read(struct nrf_rpc_tr_local_ep *local_ep, struct nrf_rpc_tr_remote_ep **src_ep, const uint8_t **buf)
{
	uint32_t len;
	uint8_t src_addr;

	NRF_RPC_ASSERT(local_ep != NULL);
	NRF_RPC_ASSERT(src_ep != NULL);
	NRF_RPC_ASSERT(buf != NULL);

	/* Make sure that buffer is released before reading next packet to
	 * prevent deadlocks.
	 */
	if (local_ep->buffer_owned) {
		NRF_RPC_WRN("Buffer should be released before");
		k_sem_give(&local_ep->done_sem);
	}

	NRF_RPC_DBG("Waiting for a packet on EP[%d]", local_ep->addr);
	do {
		k_sem_take(&local_ep->input_sem, K_FOREVER);
		len = (uint32_t)atomic_set(&local_ep->input_length,
					   (atomic_val_t)0);
	} while (len == 0);

	*src_ep = NULL;

	if (len & FLAG_FILTERED) {
		/* Packet was filtered, so allow receiving thread to continue */
		k_sem_give(&local_ep->done_sem);
		*buf = NULL;
		len ^= FLAG_FILTERED;
		NRF_RPC_DBG("Read on EP[%d] filtered %d", local_ep->addr, len);
	} else {
		src_addr = local_ep->input_buffer[HEADER_SRC_INDEX];
		if (src_addr < ARRAY_SIZE(remote_pool)) {
			*src_ep = &remote_pool[src_addr].tr_ep;
		}
		local_ep->buffer_owned = true;
		*buf = local_ep->input_buffer + NRF_RPC_TR_MAX_HEADER_SIZE;
		len -= NRF_RPC_TR_MAX_HEADER_SIZE;
		DUMP_LIMITED_DBG(*buf, len, "Read packet");
	}

	return len;
}


void nrf_rpc_tr_release_buffer(struct nrf_rpc_tr_local_ep *local_ep)
{
	if (local_ep != NULL && local_ep->buffer_owned) {
		NRF_RPC_DBG("Read filtered %d", len);
		local_ep->buffer_owned = false;
		k_sem_give(&local_ep->done_sem);
	}
}

struct nrf_rpc_tr_local_ep *nrf_rpc_tr_current_get()
{
	// TODO: not using thread custom data
	struct nrf_rpc_tr_local_ep *ep = (struct nrf_rpc_tr_local_ep *)k_thread_custom_data_get();

	if (ep >= &local_endpoints[0].tr_ep && ep < &local_endpoints[ARRAY_SIZE(local_endpoints)].tr_ep) {
		return ep;
	}

	atomic_val_t new_index = atomic_inc(&next_free_extra_ep);

	if (new_index >= ARRAY_SIZE(local_endpoints)) {
		// TODO: fatal error OR assert
		return NULL;
	}

	local_endpoints[new_index].tr_ep.custom_data = (void *)ep;
	ep = &local_endpoints[new_index].tr_ep;
	k_thread_custom_data_set(ep);

	ep->addr = new_index;
	k_sem_init(&ep->done_sem, 0, 1);
	k_sem_init(&ep->input_sem, 0, 1);

	return ep;
}

void *nrf_rpc_tr_thread_custom_data_get(void)
{
	struct nrf_rpc_tr_local_ep *ep = (struct nrf_rpc_tr_local_ep *)k_thread_custom_data_get();

	if (ep >= &local_endpoints[0].tr_ep && ep < &local_endpoints[ARRAY_SIZE(local_endpoints)].tr_ep) {
		return ep->custom_data;
	}

	return ep;
}

void nrf_rpc_tr_thread_custom_data_set(void *value)
{
	struct nrf_rpc_tr_local_ep *ep = (struct nrf_rpc_tr_local_ep *)k_thread_custom_data_get();

	if (ep >= &local_endpoints[0].tr_ep && ep < &local_endpoints[ARRAY_SIZE(local_endpoints)].tr_ep) {
		ep->custom_data = value;
	}

	k_thread_custom_data_set(value);
}

struct nrf_rpc_tr_remote_ep *nrf_rpc_tr_remote_reserve(void)
{
	sys_snode_t *node;
	struct nrf_rpc_tr_remote_ep *ep;

	k_sem_take(&remote_pool_sem, K_FOREVER);
	k_mutex_lock(&remote_pool_mutex, K_FOREVER);

	node = sys_slist_get(&remote_pool_free);
	ep = CONTAINER_OF(node, struct nrf_rpc_tr_remote_ep, node);
	ep->used = true;

	k_mutex_unlock(&remote_pool_mutex);
	
	return ep;
}

void nrf_rpc_tr_remote_release(struct nrf_rpc_tr_remote_ep *ep)
{
	printk("nrf_rpc_tr_remote_release %d\n", ep->addr);

	k_mutex_lock(&remote_pool_mutex, K_FOREVER);

	if (ep != NULL && ep->used) {
		ep->used = false;
		sys_slist_append(&remote_pool_free, &ep->node);
		k_mutex_unlock(&remote_pool_mutex);
		k_sem_give(&remote_pool_sem);
	} else {
		k_mutex_unlock(&remote_pool_mutex);
	}
}
