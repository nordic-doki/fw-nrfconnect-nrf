/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef NRF_RPC_OS_H_
#define NRF_RPC_OS_H_

#include <zephyr.h>

struct nrf_rpc_os_event
{
	struct k_sem sem;
};

struct nrf_rpc_os_msg
{
	struct k_sem sem;
	void *data;
	size_t len;
};

typedef void (*nrf_rpc_os_work_t)(void *data, size_t len);

int nrf_rpc_os_init();

int nrf_rpc_os_thread_pool_send(nrf_rpc_os_work_t work, void *data, size_t len);

static inline int nrf_rpc_os_event_init(struct nrf_rpc_os_event *event)
{
	return k_sem_init(&event->sem, 0, 1);
}

static inline int nrf_rpc_os_event_set(struct nrf_rpc_os_event *event)
{
	k_sem_give(&event->sem);
	return 0;
}

static inline int nrf_rpc_os_event_wait(struct nrf_rpc_os_event *event)
{
	k_sem_take(&event->sem, K_FOREVER);
	return 0;
}

static inline int nrf_rpc_os_msg_init(struct nrf_rpc_os_msg *msg)
{
	return k_sem_init(&msg->sem, 0, 1);
}

int nrf_rpc_os_msg_set(struct nrf_rpc_os_msg *msg, void *data, size_t len);
int nrf_rpc_os_msg_get(struct nrf_rpc_os_msg *msg, void **data, size_t *len);

static inline void* nrf_rpc_os_tls_get(void)
{
	return k_thread_custom_data_get();
}

static inline void nrf_rpc_os_tls_set(void *data)
{
	k_thread_custom_data_set(data);
}

int nrf_rpc_os_ctx_pool_reserve();
void nrf_rpc_os_ctx_pool_release(int number);

int nrf_rpc_os_remote_count(int count);

static inline int nrf_rpc_os_remote_reserve()
{
	extern struct k_sem _nrf_rpc_os_remote_counter;

	k_sem_take(&_nrf_rpc_os_remote_counter, K_FOREVER);
	return 0;
}

static inline void nrf_rpc_os_remote_release()
{
	extern struct k_sem _nrf_rpc_os_remote_counter;

	k_sem_give(&_nrf_rpc_os_remote_counter);
}

#endif /* NRF_RPC_OS_H_ */
