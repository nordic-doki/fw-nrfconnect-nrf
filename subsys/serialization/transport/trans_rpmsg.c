/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <errno.h>

#include <rp_errors.h>

#include "rp_ll.h"
#include "trans_rpmsg.h"

#include <logging/log.h>


LOG_MODULE_REGISTER(trans_rpmsg);


#define FLAG_FILTERED (0x80000000uL)


static rp_trans_receive_handler receive_handler;
static rp_trans_filter receive_filter;


static int translate_error(int rpmsg_err)
{
	switch (rpmsg_err) {
	case RPMSG_ERR_NO_MEM:
		return RP_ERROR_NO_MEM;
	case RPMSG_ERR_NO_BUFF:
		return RP_ERROR_NO_MEM;
	case RPMSG_ERR_PARAM:
		return RP_ERROR_INVALID_PARAM;
	case RPMSG_ERR_DEV_STATE:
		return RP_ERROR_INVALID_STATE;
	case RPMSG_ERR_BUFF_SIZE:
		return RP_ERROR_NO_MEM;
	case RPMSG_ERR_INIT:
		return RP_ERROR_INTERNAL;
	case RPMSG_ERR_ADDR:
		return RP_ERROR_INTERNAL;
	default:
		if (rpmsg_err < 0) {
			return RP_ERROR_INTERNAL;
		}
		break;
	}
	return RP_SUCCESS;
}

int rp_trans_init(rp_trans_receive_handler callback, rp_trans_filter filter)
{
	int result;

	__ASSERT(callback, "Callback cannot be NULL");
	receive_handler = callback;
	receive_filter = filter;
	result = rp_ll_init();
	return translate_error(result);
}

void endpoint_work(struct k_work *item)
{
	uint32_t len;
	struct rp_trans_endpoint *endpoint = CONTAINER_OF(item, struct rp_trans_endpoint, work);

	k_mutex_lock(&endpoint->mutex, K_FOREVER);
	len = (uint32_t)atomic_set(&endpoint->input_length, (atomic_val_t)0);
	if (len & FLAG_FILTERED) {
		rp_trans_release_buffer(endpoint);
		receive_handler(endpoint, NULL, len ^ FLAG_FILTERED);
	} else {
		receive_handler(endpoint, endpoint->input_buffer, len);
	}
	k_mutex_unlock(&endpoint->mutex);
}

static void event_handler(struct rp_ll_endpoint *ll_ep,
	enum rp_ll_event_type event, const u8_t *buf, size_t length)
{
	uint32_t filtered;
	struct rp_trans_endpoint *endpoint =
		CONTAINER_OF(ll_ep, struct rp_trans_endpoint, ll_ep);

	if (event == RP_LL_EVENT_CONNECTED) {
		k_sem_give(&endpoint->input_sem);
		return;
	} else if (event != RP_LL_EVENT_DATA || length == 0) {
		return;
	}

	filtered = receive_filter(endpoint, buf, length);

	if (endpoint->wait_for_done)
	{
		k_sem_take(&endpoint->done_sem, K_FOREVER);
	}

	if (!filtered) {
		/* It is safe to modify decode_buffer, because other thread will
		 * read/write it only after k_sem_give and k_sem_take. Additionally
		 * other thread will change it to NULL, so they will be NULL after
		 * k_sem_take and as the result also now. TODO: update this description
		 */
		endpoint->input_buffer = buf;
		/* Make sure that input_buffer was set before input_length */
		__DMB();
		//atomic_and(&endpoint->input_length, (atomic_val_t)0xFF000000); <-- not needed length == 0 at this point
		atomic_set(&endpoint->input_length, (atomic_val_t)length);
		/* Semafore given two times because first may give to endpoint thread
		 * even when user thread is waiting. Endpoint thread will start and
		 * then hang on mutex. This allows user thread to execute and consume
		 * data.
		 */
		/* Wait when decoding is done to safely return buffers
		 * to caller.
		 */
		endpoint->wait_for_done = false;
	} else {
		/* TODO: description*/
		atomic_set(&endpoint->input_length, (atomic_val_t)filtered | (atomic_val_t)FLAG_FILTERED);
		endpoint->wait_for_done = true;
	}

	k_sem_give(&endpoint->input_sem);
	if (!endpoint->reading) {
		k_work_submit_to_queue(&endpoint->work_queue, &endpoint->work);
	}
	endpoint->reading = false;
	
	if (!filtered) {
		k_sem_take(&endpoint->done_sem, K_FOREVER);
	}
}

int rp_trans_endpoint_init(struct rp_trans_endpoint *endpoint,
	int endpoint_number)
{
	int result;
	int prio = endpoint->prio;
	size_t stack_size = endpoint->stack_size;
	k_thread_stack_t *stack = endpoint->stack;

	endpoint->input_length = ATOMIC_INIT(0);
	endpoint->reading = false;
	endpoint->wait_for_done = false;

	k_mutex_init(&endpoint->mutex);
	k_sem_init(&endpoint->input_sem, 0, 1);
	k_sem_init(&endpoint->done_sem, 0, 1);

	result = rp_ll_endpoint_init(&endpoint->ll_ep, endpoint_number,
		event_handler, NULL);

	if (result < 0) {
		return translate_error(result);
	}

	k_sem_take(&endpoint->input_sem, K_FOREVER);

	k_work_q_start(&endpoint->work_queue, stack, stack_size, prio);
	k_work_init(&endpoint->work, endpoint_work);

	return RP_SUCCESS;
}

int rp_trans_send(struct rp_trans_endpoint *endpoint, const u8_t *buf,
	size_t buf_len)
{
	int result = rp_ll_send(&endpoint->ll_ep, buf, buf_len);

	return translate_error(result);
}

void rp_trans_own(struct rp_trans_endpoint *endpoint)
{
	k_mutex_lock(&endpoint->mutex, K_FOREVER);
}

void rp_trans_give(struct rp_trans_endpoint *endpoint)
{
	k_mutex_unlock(&endpoint->mutex);
}

int rp_trans_read(struct rp_trans_endpoint *endpoint, const uint8_t **buf)
{
	uint32_t len;

	do {
		endpoint->reading = true;
		k_sem_take(&endpoint->input_sem, K_FOREVER);
		len = (uint32_t)atomic_set(&endpoint->input_length, (atomic_val_t)0);
	} while (len == 0);

	if (len & FLAG_FILTERED) {
		rp_trans_release_buffer(endpoint);
		*buf = NULL;
		len ^= FLAG_FILTERED;
	} else {
		*buf = endpoint->input_buffer;
	}

	return len;
}

void rp_trans_release_buffer(struct rp_trans_endpoint *endpoint)
{
	k_sem_give(&endpoint->done_sem);
}
