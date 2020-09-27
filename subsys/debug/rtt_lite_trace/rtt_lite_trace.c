/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <tracing/tracing.h>
#include <ksched.h>
#include <SEGGER_RTT.h>
#include <nrfx_timer.h>
#include <stdarg.h>

#include "rtt_lite_trace_internal.h"

#ifndef _RTT_LIGHT_TRACE_H
#error RTT lite system tracing enabled, but not selected.
#error Select CONFIG_TRACING_CUSTOM option.
#error Make sure that CONFIG_TRACING_CUSTOM_INCLUDE is "debug/rtt_lite_trace.h".
#endif

#define FORMAT_ARG_END 0
#define FORMAT_ARG_INT32 1
#define FORMAT_ARG_INT64 2
#define FORMAT_ARG_STRING 3

#if defined CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_512B
#define RTT_BUFFER_BYTES 512
#elif defined CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_1KB
#define RTT_BUFFER_BYTES 1024
#elif defined CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_2KB
#define RTT_BUFFER_BYTES 2048
#elif defined CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_4KB
#define RTT_BUFFER_BYTES 4096
#elif defined CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_8KB
#define RTT_BUFFER_BYTES 8192
#elif defined CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_16KB
#define RTT_BUFFER_BYTES 16384
#elif defined CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_32KB
#define RTT_BUFFER_BYTES 32768
#elif defined CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_64KB
#define RTT_BUFFER_BYTES 65536
#elif defined CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_128KB
#define RTT_BUFFER_BYTES 131072
#else
#error CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_xyz not defined!
#endif

#if defined CONFIG_RTT_LITE_TRACE_TIMER0
static const nrfx_timer_t timer = NRFX_TIMER_INSTANCE(0);
#define NRF_TIMER_INSTANCE NRF_TIMER0
#elif defined CONFIG_RTT_LITE_TRACE_TIMER1
static const nrfx_timer_t timer = NRFX_TIMER_INSTANCE(1);
#define NRF_TIMER_INSTANCE NRF_TIMER1
#elif defined CONFIG_RTT_LITE_TRACE_TIMER2
static const nrfx_timer_t timer = NRFX_TIMER_INSTANCE(2);
#define NRF_TIMER_INSTANCE NRF_TIMER2
#elif defined CONFIG_RTT_LITE_TRACE_TIMER3
static const nrfx_timer_t timer = NRFX_TIMER_INSTANCE(3);
#define NRF_TIMER_INSTANCE NRF_TIMER3
#elif defined CONFIG_RTT_LITE_TRACE_TIMER4
static const nrfx_timer_t timer = NRFX_TIMER_INSTANCE(4);
#define NRF_TIMER_INSTANCE NRF_TIMER4
#else
#error CONFIG_RTT_LITE_TRACE_TIMERx not defined!
#endif


#define RTT_BUFFER_INDEX (*(volatile unsigned int*) \
		(&_SEGGER_RTT.aUp[CONFIG_RTT_LITE_TRACE_RTT_CHANNEL].WrOff))
#define RTT_BUFFER_READ_INDEX (*(volatile unsigned int*) \
		(&_SEGGER_RTT.aUp[CONFIG_RTT_LITE_TRACE_RTT_CHANNEL].RdOff))
#define RTT_BUFFER_WORDS (RTT_BUFFER_BYTES / sizeof(uint32_t))
#define RTT_BUFFER_INDEX_MASK (RTT_BUFFER_BYTES - 1)
#define RTT_BUFFER_U8(byte_index) (((volatile uint8_t*) \
		(&rtt_buffer))[byte_index])
#define RTT_BUFFER_U32(byte_index) (*(volatile uint32_t*) \
		(&RTT_BUFFER_U8(byte_index)))

#define INIT_SEND_BUFFER_CONTEXT { .used = 0, .data = { 0, EV_BUFFER_BEGIN } }

/* It is easier to use short internal event names instead of public ones with
 * long prefix. That is why they are doubled. Following asserts makes sure that
 * the values are aligned between public and internal header.
 */
BUILD_ASSERT(EV_MARK_START == _RTT_LITE_TRACE_EV_MARK_START,
	     "Misaligned event numbers in public and internal header file!");
BUILD_ASSERT(EV_MARK == _RTT_LITE_TRACE_EV_MARK,
	     "Misaligned event numbers in public and internal header file!");
BUILD_ASSERT(EV_MARK_STOP == _RTT_LITE_TRACE_EV_MARK_STOP,
	     "Misaligned event numbers in public and internal header file!");
BUILD_ASSERT(EV_NUMBER_FIRST_USER == _RTT_LITE_TRACE_EV_NUMBER_FIRST_USER,
	     "Misaligned event numbers in public and internal header file!");
BUILD_ASSERT(EV_NUMBER_FIRST_RESERVED ==
	     _RTT_LITE_TRACE_EV_NUMBER_FIRST_RESERVED,
	     "Misaligned event numbers in public and internal header file!");

struct send_buffer_context {
	size_t used;
	uint32_t data[2];
};


static uint32_t rtt_buffer[RTT_BUFFER_WORDS + 4];


static ALWAYS_INLINE uint32_t get_isr_number(void)
{
	return __get_IPSR();
}

static ALWAYS_INLINE uint32_t get_time(void)
{
	NRF_TIMER_INSTANCE->TASKS_CAPTURE[0] = 1;
	return NRF_TIMER_INSTANCE->CC[0];
}

static ALWAYS_INLINE void send_event_inner(uint32_t event, uint32_t param, uint32_t time,
		bool with_param)
{
	uint32_t index;
	uint32_t left;
	uint32_t cnt;
	int key;

	event = event | time;

	key = irq_lock();

	index = RTT_BUFFER_INDEX;

	if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_FAST_OVERFLOW_CHECK)) {

		RTT_BUFFER_U32(index) = event;
		if (with_param) {
			RTT_BUFFER_U32(index + 4) = param;
		}
		index = index + 8;
		if (index == RTT_BUFFER_BYTES) {
			RTT_BUFFER_U32(RTT_BUFFER_BYTES + 4) += 2;
			index = 0;
		}

	} else {

		left = (RTT_BUFFER_READ_INDEX - index - 1)
				& (RTT_BUFFER_INDEX_MASK & ~7);

		if (left <= 8) {
			if (left == 0) {
				cnt = (index - 4) & RTT_BUFFER_INDEX_MASK;
				RTT_BUFFER_U32(cnt)++;
				goto unlock_and_return;
			}
			event = EV_OVERFLOW | get_time();
			param = 1;
		}
		RTT_BUFFER_U32(index) = event;
		RTT_BUFFER_U32(index + 4) = param;
		index = (index + 8) & RTT_BUFFER_INDEX_MASK;

		if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_BUFFER_STATS)) {
			left -= 8;
			if (left < RTT_BUFFER_U32(RTT_BUFFER_BYTES + 4)) {
				RTT_BUFFER_U32(RTT_BUFFER_BYTES + 4) = left;
			}
		}
	}

	RTT_BUFFER_INDEX = index;

unlock_and_return:
	irq_unlock(key);
}

static void send_event(uint32_t event, uint32_t param)
{
	send_event_inner(event, param, get_time(), true);
}

static void send_timeless(uint32_t event, uint32_t param)
{
	send_event_inner(event, param, 0, true);
}

static void send_short(uint32_t event)
{
	send_event_inner(event, 0, get_time(), false);
}

static void send_buffers(struct send_buffer_context *buf, const void *data,
		size_t size)
{
	size_t left;
	const uint8_t *src = (uint8_t *)data;
	uint8_t *dst;

	while (size > 0) {
		dst = &((uint8_t *)&buf->data[0])[buf->used];
		left = 7 - buf->used;
		if (size < left) {
			left = size;
		}
		memcpy(dst, src, left);
		buf->used += left;
		src += left;
		size -= left;
		if (buf->used == 7) {
			send_timeless(buf->data[1], buf->data[0]);
			buf->used = 0;
			buf->data[1] &= 0x00FFFFFF;
			buf->data[1] |= EV_BUFFER_NEXT;
		}
	}
}

static void done_buffers(struct send_buffer_context *buf)
{
	uint32_t event = buf->data[1] & 0xFF000000;

	buf->data[1] &= 0x0000FFFF;
	buf->data[1] |= buf->used << 16;
	if (event == EV_BUFFER_BEGIN) {
		buf->data[1] |= EV_BUFFER_BEGIN_END;
	} else {
		buf->data[1] |= EV_BUFFER_END;
	}
	send_timeless(buf->data[1], buf->data[0]);
	buf->used = 0;
	buf->data[1] = EV_BUFFER_BEGIN;
}

static void send_thread_info(k_tid_t thread)
{
	uint32_t param = 0;
	struct send_buffer_context buf = INIT_SEND_BUFFER_CONTEXT;

#if defined(CONFIG_THREAD_STACK_INFO)
	{
		uint32_t size = thread->stack_info.size;
		uint32_t start = thread->stack_info.start;

		send_buffers(&buf, &size, sizeof(size));
		send_buffers(&buf, &start, sizeof(start));
		param = THREAD_INFO_STACK_PRESENT;
	}
#endif /* CONFIG_THREAD_STACK_INFO */

	if (IS_ENABLED(CONFIG_THREAD_NAME)) {
		const uint8_t *name = (const uint8_t *)k_thread_name_get(thread);

		if (name != NULL && name[0] != 0) {
			send_buffers(&buf, name, strlen(name));
			param |= THREAD_INFO_NAME_PRESENT;
		}
	}

	if (param) {
		done_buffers(&buf);
	}

	param |= (uint32_t)(uint8_t)thread->base.prio;
	if (z_is_idle_thread_object(thread)) {
		param |= THREAD_INFO_IDLE;
	}

	send_timeless(EV_THREAD_INFO | param, (uint32_t)thread);
}

static void send_periodic_thread_info(void)
{
	static struct k_thread *next_thread; /* zero-initialized after reset */
	struct k_thread *thread = NULL;

	for (thread = _kernel.threads; thread; thread = thread->next_thread) {
		if (thread == next_thread) {
			break;
		}
	}
	if (!thread) {
		thread = _kernel.threads;
	}
	next_thread = thread->next_thread;
	send_thread_info(thread);
}

static void initialize(void)
{
	static bool initialized; /* zero-initialized after reset */

	if (!initialized) {
		nrfx_timer_config_t timer_conf = NRFX_TIMER_DEFAULT_CONFIG;
		SEGGER_RTT_BUFFER_UP *up;

		/*
		 * Directly initialize RTT up channel to avoid unexpected traces
		 * before initialization.
		 */
		up = &_SEGGER_RTT.aUp[CONFIG_RTT_LITE_TRACE_RTT_CHANNEL];
		up->sName = CHANNEL_NAME;
		up->pBuffer = (char *)(&rtt_buffer[0]);
		up->SizeOfBuffer = sizeof(rtt_buffer);
		up->RdOff = 0u;
		up->WrOff = 0u;
		up->Flags = SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL;

		RTT_BUFFER_U32(RTT_BUFFER_BYTES) = EV_CYCLE;
		if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_FAST_OVERFLOW_CHECK)) {
			RTT_BUFFER_U32(RTT_BUFFER_BYTES + 4) = 1;
		} else {
			RTT_BUFFER_U32(RTT_BUFFER_BYTES + 4) = RTT_BUFFER_BYTES;
		}

		timer_conf.frequency = NRF_TIMER_FREQ_16MHz;
		timer_conf.bit_width = NRF_TIMER_BIT_WIDTH_24;

		nrfx_timer_init(&timer, &timer_conf, NULL);
		nrfx_timer_enable(&timer);

		RTT_BUFFER_U32(RTT_BUFFER_BYTES + 8) = EV_SYNC | SYNC_ADDITIONAL;
		RTT_BUFFER_U32(RTT_BUFFER_BYTES + 12) = SYNC_PARAM;
		send_timeless(EV_SYNC | SYNC_ADDITIONAL, SYNC_PARAM);
		send_event(EV_SYSTEM_RESET, 0);

		initialized = true;
	}
}

void sys_trace_thread_switched_in(void)
{
	send_event(EV_THREAD_START, (uint32_t)k_current_get());
}

void sys_trace_thread_switched_out(void)
{
	send_short(EV_THREAD_STOP);
}

void sys_trace_isr_enter(void)
{
	if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_IRQ)) {
		send_short(EV_ISR_ENTER | (get_isr_number() << 24));
	}
}

void sys_trace_isr_exit(void)
{
	if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_IRQ)) {
		send_short(EV_ISR_EXIT);
	}
}

void sys_trace_isr_exit_to_scheduler(void)
{
	if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_IRQ)) {
		send_short(EV_ISR_EXIT_TO_SCHEDULER);
	}
}


void sys_trace_idle(void)
{
	send_timeless(EV_SYNC | SYNC_ADDITIONAL, SYNC_PARAM);
	send_event(EV_IDLE, RTT_BUFFER_U32(RTT_BUFFER_BYTES + 4));
	if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_THREAD_INFO)) {
		send_periodic_thread_info();
	}
}

void sys_trace_thread_priority_set(k_tid_t thread)
{
	uint8_t prio = (uint8_t)thread->base.prio;
	uint32_t idle = z_is_idle_thread_object(thread) ? 0x10000 : 0;

	send_timeless(EV_THREAD_INFO | (uint32_t)prio | idle, (uint32_t)thread);
}

void sys_trace_thread_create(k_tid_t thread)
{
	initialize();
	send_event(EV_THREAD_CREATE, (uint32_t)thread);
	if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_THREAD_INFO)) {
		send_thread_info(thread);
	} else {
		sys_trace_thread_priority_set(thread);
	}
}

void sys_trace_thread_suspend(k_tid_t thread)
{
	send_event(EV_THREAD_SUSPEND, (uint32_t)thread);
}

void sys_trace_thread_resume(k_tid_t thread)
{
	send_event(EV_THREAD_RESUME, (uint32_t)thread);
}

void sys_trace_thread_ready(k_tid_t thread)
{
	send_event(EV_THREAD_READY, (uint32_t)thread);
}

void sys_trace_thread_pend(k_tid_t thread)
{
	send_event(EV_THREAD_PEND, (uint32_t)thread);
}

#ifdef CONFIG_RTT_LITE_TRACE_THREAD_INFO

void sys_trace_thread_name_set(k_tid_t thread)
{
	send_thread_info(thread);
}

#endif /* CONFIG_RTT_LITE_TRACE_THREAD_INFO */

#ifdef CONFIG_RTT_LITE_TRACE_SYNCHRO

void sys_trace_void(uint32_t id)
{
	send_event(EV_SYS_CALL, id);
}

void sys_trace_end_call(uint32_t id)
{
	send_event(EV_SYS_END_CALL, id);
}

#endif /* CONFIG_RTT_LITE_TRACE_SYNCHRO */

static uint8_t parse_format_arg(const char **pp)
{
	static const uint8_t table[] = {
		/*    !  "  #  $  %  &  '  (  )  *  +  ,  -  .  / */
		   9, 0, 0, 9, 9, 0, 0, 0, 0, 0, 0, 9, 0, 9, 9, 0,
		/* 0  9  2  3  4  5  6  7  8  9  :  ;  <  =  >  ? */
		   9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 0, 0, 0, 0, 0, 0,
		/* @  A  B  C  D  E  F  G  H  I  J  K  L  M  N  O */
		   0, 0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0,
		/* P  Q  R  S  T  U  V  W  X  Y  Z  [  \  ]  ^  _ */
		   0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0,
		/* `  a  b  c  d  e  f  g  h  i  j  k  l  m  n  o */
		   0, 0, 0, 7, 7, 2, 2, 2, 9, 7, 0, 0, 8, 0, 0, 7,
		/* p  q  r  s  t  u  v  w  x  y  z  {  |  }  ~    */
		   7, 0, 0, 3, 0, 7, 0, 0, 7,/*0,0, 0, 0, 0, 0, 0,*/
	};
	const char *p = *pp;
	int long_count = 0;
	uint8_t type;
	uint8_t result = FORMAT_ARG_END;

	while (*p) {
		type = (*p < ' ' || *p > 'x') ? 0 : table[*p - ' '];
		p++;
		if (type < 7) {
			result = type;
			break;
		} else if (type == 7) {
			result = (long_count <= 1) ? FORMAT_ARG_INT32 :
				(long_count == 2) ? FORMAT_ARG_INT64 :
				FORMAT_ARG_END;
			break;
		} else if (type == 8) {
			long_count++;
		}
	}
	*pp = p;
	return result;
}

static void parse_format_args(struct rtt_lite_trace_format *format)
{
	const char *p = format->text;
	uint8_t *arg = &format->args[0];
	uint8_t *arg_last = &format->args[sizeof(format->args) - 1];

	while (*p && arg < arg_last) {
		if (*p == '%') {
			p++;
			if (*p == '%') {
				p++;
			} else {
				*arg = parse_format_arg(&p);
				arg++;
			}
		} else {
			p++;
		}
	}
	*arg = FORMAT_ARG_END;
}

static void prepare_format(struct rtt_lite_trace_format *format)
{
	static volatile uint32_t last_format_id; /* zero-initialied after reset */
	uint32_t this_format_id;
	int key;
	uint32_t id_high;

	parse_format_args(format);

	id_high = (uint32_t)format->level << 24;
	if (!IS_ENABLED(CONFIG_RTT_LITE_TRACE_FORMAT_ONCE) || format->args[0]) {
		id_high |= 0x80000000;
	}

	if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_FORMAT_ONCE)) {
		struct send_buffer_context buf = INIT_SEND_BUFFER_CONTEXT;

		key = irq_lock();
		this_format_id = last_format_id + 1;
		last_format_id = this_format_id;
		irq_unlock(key);

		this_format_id |= id_high;
		send_buffers(&buf, format->text, strlen(format->text));
		done_buffers(&buf);
		send_timeless(EV_FORMAT, this_format_id);
		format->id = this_format_id;
	} else {
		format->id = 0x00FFFFFF | id_high;
	}
}

void rtt_lite_trace_printf(struct rtt_lite_trace_format *format, ...)
{
	if (format->id == 0) {
		prepare_format(format);
	}

	if (format->id & 0x80000000) {
		uint32_t val32;
		uint64_t val64;
		const char *val_str;
		va_list vl;
		uint8_t *p;
		struct send_buffer_context buf = INIT_SEND_BUFFER_CONTEXT;

		if (!IS_ENABLED(CONFIG_RTT_LITE_TRACE_FORMAT_ONCE)) {
			send_buffers(&buf, format->text, strlen(format->text) + 1);
		}
		p = format->args;
		va_start(vl, format);
		while (*p != FORMAT_ARG_END) {
			switch (*p) {
			case FORMAT_ARG_INT32:
				val32 = va_arg(vl, uint32_t);
				send_buffers(&buf, &val32, 4);
				break;
			case FORMAT_ARG_INT64:
				val64 = va_arg(vl, uint64_t);
				send_buffers(&buf, &val64, 8);
				break;
			case FORMAT_ARG_STRING:
				val_str = va_arg(vl, const char *);
				send_buffers(&buf, val_str, strlen(val_str) + 1);
				break;
			}
			p++;
		}
		va_end(vl);
		done_buffers(&buf);
	}

	send_event(EV_PRINTF, format->id);
}

uint32_t rtt_lite_trace_time(void)
{
	return get_time();
}

void rtt_lite_trace_print(uint32_t level, const char *text)
{
	struct send_buffer_context buf = INIT_SEND_BUFFER_CONTEXT;

	send_buffers(&buf, text, strlen(text));
	done_buffers(&buf);
	send_event(EV_PRINT, level);
}

void rtt_lite_trace_event(uint32_t event, uint32_t param)
{
	send_event(event, param);
}

void rtt_lite_trace_call_v(uint32_t event, uint32_t num_args, uint32_t arg1, ...)
{
	uint32_t i;
	va_list vl;
	uint32_t val;
	struct send_buffer_context buf = INIT_SEND_BUFFER_CONTEXT;

	va_start(vl, arg1);
	for (i = 1; i < num_args; i++) {
		val = va_arg(vl, uint32_t);
		send_buffers(&buf, &val, 4);
	}
	va_end(vl);
	done_buffers(&buf);
	rtt_lite_trace_event(event, arg1);
}

void rtt_lite_trace_name(uint32_t resource_id, const char *name)
{
	struct send_buffer_context buf = INIT_SEND_BUFFER_CONTEXT;

	send_buffers(&buf, name, strlen(name));
	done_buffers(&buf);
	send_timeless(EV_RES_NAME, resource_id);
}
