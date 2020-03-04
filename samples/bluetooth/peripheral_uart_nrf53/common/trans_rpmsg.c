
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

typedef void (*enum_services_callback)(const char* name);

// callback called from transport receive thread
static void rx_callback(struct rpser_endpoint *ep, u8_t* buf, size_t length)
{
	// it is safe to modify it, because other thread will read/write it
	// only after give(decode_sem) and before take(decode_done_sem)
	ep->decode_buffer = buf;
	ep->decode_length = length;
	// Two times because first may give to endpoint thread even when
	// other thread is waiting. Endpoint thread will start and hang on mutex
	// lock in this situation. This allows other thread to execute and
	// consume data
	k_sem_give(&ep->decode_sem);
	k_sem_give(&ep->decode_sem);
	// Wait when decoding is done to safely return buffers to caller
	k_sem_take(&ep->decode_done_sem, K_FOREVER);
}

// thread for each endpoint
static void rpser_endpoint_thread(void *p1, void *p2, void *p3)
{
	struct rpser_endpoint *ep = (struct rp_trans_endpoint *)p1;

	while (1) {
		// wait for incoming packets
		k_sem_take(&ep->decode_sem, K_FOREVER);
		if (ep->decode_buffer) {
			// lock entire endpoint, so no other thread can access it
			k_mutex_lock(ep->mutex, K_FOREVER);
			// consume incoming data
			u8_t *a = ep->decode_buffer;
			size_t b = ep->decode_length;
			ep->decode_buffer = NULL;
			ep->decode_length = 0;
			handle_packet(a, b);
			k_mutex_unlock(ep->mutex);
		}
	}
}

// called from endpoint thread or loop waiting for response
void handle_packet(u8_t* data, size_t length)
{
	packet_type type = data[0];
	// We get response - this kind of packet should be handled before
	__ASSERT(type != PACKET_TYPE_RESPONSE, "Response packet without any call");

	// This contition may only be true in endpoint thread (maybe add assert?)
	if (type == PACKET_TYPE_ACK) {
		ep->waiting_for_ack = false;
		return;
	}

	decoder_callback decoder = get_decoder_from_data(data);

	// If we are executing command then the other end is waiting for
	// response, so sending notifications and commands is available again now.
	bool prev_wait_for_ack = ep->waiting_for_ack;
	if (packet_type == PACKET_TYPE_CMD) {
		ep->waiting_for_ack = false;
	}

	decoder(type, &data[5], length - 5);

	// Resore previous state of waiting for ack
	if (packet_type == PACKET_TYPE_CMD) {
		ep->waiting_for_ack = prev_wait_for_ack;
	}
}

// Waits for feedback packet (response or ack) packet.
void wait_for_feedback(struct rpser_endpoint *ep, packet_type expected_type)
{
	packet_type type;
	do {
		// Wait for something from rx callback
		k_sem_take(&ep->decode_sem);
		// semafore may be given two times, so make sure that buffer is valid
		if (ep->decode_buffer) {
			type = item->buffer[0];
			// If we are waiting for response following are expected:
			//      PACKET_TYPE_CMD, PACKET_TYPE_NOTIFY - execute now
			//      PACKET_TYPE_RESPONSE - exit this function
			//      PACKET_TYPE_ACK - invalid: if we are wating for response then we already get ack before
			// If we are waiting for ack:
			//      PACKET_TYPE_CMD, PACKET_TYPE_NOTIFY - execute now
			//      PACKET_TYPE_ACK - exit this function
			//      PACKET_TYPE_RESPONSE - invalid: handling of notification is not finished yet, so no command was send before
			if (type == expected_type) {
				break;
			} else {
				__ASSERT(type == PACKET_TYPE_CMD || type == PACKET_TYPE_NOTIFY, "Feedback packet different than expected");
				u8_t *a = ep->decode_buffer;
				size_t b = ep->decode_length;
				ep->decode_buffer = NULL;
				ep->decode_length = 0;
				handle_packet(a, b);
			}
		}
	} while (true);
}

// Call after send of command to wait for response
void wait_for_response(struct rpser_endpoint *ep, struct decode_buffer *out)
{
	// Wait for response packet
	wait_for_feedback(ep, PACKET_TYPE_RESPONSE);
	// Consume buffer (decode done will be send later)
	out->buffer = ep->decode_buffer;
	out->length = ep->decode_length;
	ep->decode_buffer = NULL;
	ep->decode_length = 0;
}

// Called before sending command or notify to make sure that last notification was finished and the other end
// can handle this packet imidetally.
void wait_for_last_ack(struct rpser_endpoint *ep)
{
	if (ep->waiting_for_ack)
	{
		// Wait for ack packet
		wait_for_feedback(ep, PACKET_TYPE_ACK);
		// Tell rx callback that decoding is not and it may continue
		k_sem_give(ep->decode_done_sem);
		// We are not wating anymore
		ep->waiting_for_ack = false;
	}
}

// Call remote function (command): send cmd and data, wait and return response
void call_remote(struct rpser_endpoint *ep, struct decode_buffer *in, struct decode_buffer *out)
{
	// Endpoint is not accessible by other thread from this point
	k_mutex_lock(ep->mutex, K_FOREVER);
	// Make sure that someone can handle packet immidietallty
	wait_for_last_ack(ep);
	// Send buffer to transport layer
	rp_trans_send(&ep->trans_ep, in->data, in->length);
	// Wait for response. During waiting nested commands and notifications are possible
	wait_for_response(ep, out);
	// Decoding and sednig 'decode done' to callback is in decode_response_done
}

// Execute notification: send notification, do not wait - only mark that we are expecting ack later
void notify_remote(struct rpser_endpoint *ep, struct decode_buffer *in)
{
	// Endpoint is not accessible by other thread from this point
	k_mutex_lock(ep->mutex, K_FOREVER);
	// Make sure that someone can handle packet immidietallty
	wait_for_last_ack(ep);
	// we are expecting ack later
	ep->waiting_for_ack = true;
	// Send buffer to transport layer
	rp_trans_send(&ep->trans_ep, in->data, in->length);
	// We can unlock now, nothing more to do
	k_mutex_unlock(ep->mutex);
}

// Inform that decoding is done and nothing more must be done
void decode_response_done(struct rpser_endpoint *ep)
{
	k_sem_give(ep->decode_done_sem);
	k_mutex_unlock(ep->mutex);
}

// example function
int enum_services(enum_services_callback callback, int max_count)
{
	int result;
	struct encode_buffer buf;
	struct decode_buffer dec;
	// encode params
	encode_init_cmd(&buf, ENUM_SERVICES_ID);
	encode_ptr(&buf, callback);
	encode_int(&buf, max_count);
	// call and wait
	call_remote(&my_ep, &buf, &dec);
	// decode result
	decode_int(&dec, &result);
	// inform rx callback that decoding is done and it can continue (discard buffers)
	decode_response_done(&my_ep);
	return result;
}

// example notification
void notify_update(int count)
{
	struct encode_buffer buf;
	// encode params
	encode_init_notification(&buf, NOTIFY_UPDATE_ID);
	encode_int(&buf, count);
	// send notification
	notify_remote(&my_ep, &buf, &dec);
}

// called by decoder to inform rx callback that decoding is done and it can continue (discard buffers)
void decode_params_done(struct rpser_endpoint *ep)
{
	k_sem_give(ep->decode_done_sem);
}

// example decoder (universal: for both commands and notifications)
void call_callback_b_s(struct rpser_endpoint *ep, struct decode_buffer *dec)
{
	bool result;
	char str[32];
	enum_services_callback callback;
	struct encode_buffer buf;
	// decode params
	decode_ptr(dec, &callback);
	decode_str(dec, str, 32);
	// inform rx callback that decoding is done and it can continue (discard buffers)
	decode_params_done(ep);
	// execute actual code
	result = callback(name);
	if (type == PACKET_TYPE_CMD) {
		// cndode response
		encode_init_response(&buf);
		encode_bool(&buf, result);
		// and send response
		send_response(ep, buf);
	} else {
		// send that notification is done
		send_notify_ack(ep);
	}
}
