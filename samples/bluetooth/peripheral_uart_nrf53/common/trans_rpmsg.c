
/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * This library enables user to use open-amp library to enable communication
 * between master and remote core of the processor.
 * To use:
 * 1. Register RX callback by calling ipc_register_rx_callback()
 * 2. Initialize ipc, by calling ipc_init()
 *
 * When those steps are completed, application is ready to exchange data
 * between two cores.
 */
#include <zephyr.h>
#include <errno.h>

#include "rp_ll_api.h"

#include "trans_rpmsg.h"

#include <logging/log.h>

#define CONFIG_RPMSG_LOG_LEVEL 10

LOG_MODULE_REGISTER(trans_rpmsg, CONFIG_RPMSG_LOG_LEVEL);

struct fifo_item {
	void *fifo_reserved;
	size_t size;
};

static rp_trans_receive_handler receive_handler;


static void *stack_allocate(k_thread_stack_t **stack, size_t stack_size)
{
	u8_t* buffer = k_malloc(K_THREAD_STACK_LEN(stack_size) + STACK_ALIGN + 1);
	uintptr_t ptr = (uintptr_t)buffer;
	uintptr_t unaligned = ptr % STACK_ALIGN;
	LOG_DBG("stack_allocate %d 0x%08X", K_THREAD_STACK_LEN(stack_size) + STACK_ALIGN + 1, (intptr_t)buffer);
	if (buffer == NULL) {
		*stack = NULL;
	} else if (unaligned == 0) {
		*stack = (k_thread_stack_t *)buffer;
	} else {
		*stack = (k_thread_stack_t *)(&buffer[STACK_ALIGN - unaligned]);
	}
	return buffer;
}

int rp_trans_init(rp_trans_receive_handler callback)
{
	receive_handler = callback;
	return rp_ll_init();
}

void rp_trans_uninit(void)
{
	rp_ll_uninit();
}

static void endpoint_thread(void *p1, void *p2, void *p3)
{
	struct rp_trans_endpoint *endpoint = (struct rp_trans_endpoint *)p1;
	struct fifo_item *item;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		k_sem_take(&endpoint->sem, K_FOREVER);
		if (!endpoint->running) {
			break;
		}
		LOG_DBG("RX thread signaled!");
		do {
			item = k_fifo_get(&endpoint->fifo, K_NO_WAIT);
			if (item == NULL) {
				break;
			}
			receive_handler(endpoint, (const u8_t*)(&item[1]),
				item->size);
			k_free(item);
		} while (true);
	}
}

void event_handler(struct rp_ll_endpoint *ep, enum rp_ll_event_type event,
	const u8_t *buf, size_t length)
{
	struct rp_trans_endpoint *endpoint =
		CONTAINER_OF(ep, struct rp_trans_endpoint, ep);
	
	if (event == RP_LL_EVENT_DATA) {
		LOG_DBG("RP_LL_EVENT_DATA");
		struct fifo_item *item =
			k_malloc(sizeof(struct fifo_item) + length);
		if (!item) {
			LOG_ERR("Out of memory when receiving incoming packet");
			__ASSERT(item, "Out of memory");
		}
		memcpy(&item[1], buf, length);
		item->size = length;
		k_fifo_put(&endpoint->fifo, item);
	} else {
		LOG_DBG("RP_LL_EVENT_[other]");
	}

	k_sem_give(&endpoint->sem);
}


int rp_trans_endpoint_init(struct rp_trans_endpoint *endpoint,
	int endpoint_number, size_t stack_size, int prio)
{
	endpoint->stack_buffer = stack_allocate(&endpoint->stack, stack_size);
	if (endpoint->stack_buffer == NULL) {
		LOG_ERR("Cannot allocate stack for endpoint rx thread!");
		return -ENOMEM;
	}

	endpoint->running = true;
	
	k_fifo_init(&endpoint->fifo);

	k_sem_init(&endpoint->sem, 0, 1);

	rp_ll_endpoint_init(&endpoint->ep, endpoint_number, event_handler,
		NULL); // TODO: result
	
	k_sem_take(&endpoint->sem, K_FOREVER);

	k_thread_create(&endpoint->thread, endpoint->stack, stack_size,
		endpoint_thread, endpoint, NULL, NULL, prio, 0, K_NO_WAIT);
	
	return 0;
}

void rp_trans_endpoint_uninit(struct rp_trans_endpoint *endpoint)
{
	struct fifo_item *item;

	do {
		endpoint->running = false;
		k_sem_give(&endpoint->sem);
		if (!strcmp(k_thread_state_str(&endpoint->thread), "dead")) {
			break;
		}
		k_sleep(10);
	} while (true);

	k_thread_abort(&endpoint->thread);
	rp_ll_endpoint_uninit(&endpoint->ep);

	do {
		item = k_fifo_get(&endpoint->fifo, K_NO_WAIT);
		if (item == NULL) {
			break;
		}
		k_free(item);
	} while (true);

	k_free(endpoint->stack_buffer);
}

int rp_trans_send(struct rp_trans_endpoint *endpoint, const u8_t *buf,
	size_t buf_len)
{
	return rp_ll_send(&endpoint->ep, buf, buf_len);
}

//=============================================

struct rp_trans_endpoint ep0;

void my_receive_handler(struct rp_trans_endpoint *endpoint, const u8_t *buf, size_t length)
{
	char tmp[128];
	memcpy(tmp, buf, length);
	tmp[length] = 0;
	printk("== RECV '%s'\n", tmp);
}

	
void rp_test()
{
	int err;

	if (IS_ENABLED(CONFIG_RPMSG_MASTER)) {
		printk("=== MASTER\n");
	} else {
		printk("=== SLAVE\n");
	}

	rp_trans_init(my_receive_handler);

	rp_trans_endpoint_init(&ep0, 0, 1000, 7);

	printk("Sending\n");
	
	rp_trans_send(&ep0, "123", 3);

	printk("DONE\n");
}
