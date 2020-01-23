/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <debug/rtt_lite_trace.h>
#include <ksched.h>
#include <SEGGER_RTT.h>
#include <nrfx_timer.h>


/*
 * Events without time stamp.
 * Bit format:
 *     0000 0000 eeee eeee pppp pppp pppp pppp
 *     e - event numer
 *     p - additional parameter
*/
#define EV_BUFFER_CYCLE 0x00010000
#define EV_THREAD_PRIORITY 0x00020000
#define EV_THREAD_INFO_BEGIN 0x00030000
#define EV_THREAD_INFO_NEXT 0x00040000
#define EV_THREAD_INFO_END 0x00050000

/*
 * Events with time stamp.
 * Bit format:
 *     0eee eeee tttt tttt tttt tttt tttt tttt
 *     e - event numer
 *     t - time stamp
*/
#define EV_SYSTEM_RESET 0x01000000
#define EV_BUFFER_OVERFLOW 0x02000000
#define EV_IDLE 0x03000000
#define EV_THREAD_START 0x04000000
#define EV_THREAD_STOP 0x05000000
#define EV_THREAD_CREATE 0x06000000
#define EV_THREAD_SUSPEND 0x07000000
#define EV_THREAD_RESUME 0x08000000
#define EV_THREAD_READY 0x09000000
#define EV_THREAD_PEND 0x0A000000
#define EV_SYS_CALL 0x0B000000
#define EV_SYS_END_CALL 0x0C000000
#define EV_ISR_EXIT 0x0D000000

/*
 * Events with time stamp and ISR number.
 * Bit format:
 *     eiii iiii tttt tttt tttt tttt tttt tttt
 *     e - event numer
 *     i - ISR number
 *     t - time stamp
*/
#define EV_ISR_ENTER 0x80000000


/** RTT channel name used to identify transfer channel. */
#define CHANNEL_NAME "NrfLiteTrace"


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
#else
#error CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_x not defined!
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
#define RTT_BUFFER_WORDS (RTT_BUFFER_BYTES / sizeof(u32_t))
#define RTT_BUFFER_INDEX_MASK (RTT_BUFFER_BYTES - 1)
#define RTT_BUFFER_U8(byte_index) (((volatile u8_t*) \
		(&rtt_buffer))[byte_index])
#define RTT_BUFFER_U32(byte_index) (*(volatile u32_t*) \
		(&RTT_BUFFER_U8(byte_index)))


static u32_t rtt_buffer[RTT_BUFFER_WORDS + 2];


static ALWAYS_INLINE u32_t get_isr_number()
{
	return __get_IPSR();
}

static ALWAYS_INLINE u32_t get_time()
{
	NRF_TIMER_INSTANCE->TASKS_CAPTURE[0] = 1;
	return NRF_TIMER_INSTANCE->CC[0];
}

static ALWAYS_INLINE void send_event_inner(u32_t event, u32_t param, u32_t time,
		bool with_param)
{
	int key = irq_lock();

	event = event | time;

	u32_t index = RTT_BUFFER_INDEX;

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

		u32_t left = (RTT_BUFFER_READ_INDEX - index - 1)
				& (RTT_BUFFER_INDEX_MASK & ~7);

		if (left <= 8) {
			if (left == 0) {
				u32_t cnt = (index - 4) & RTT_BUFFER_INDEX_MASK;
				RTT_BUFFER_U32(cnt)++;
				goto unlock_and_return;
			}
			event = EV_BUFFER_OVERFLOW | get_time();
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

static void send_event(u32_t event, u32_t param)
{
	u32_t time = get_time();
	send_event_inner(event, param, time, true);
}

static void send_timeless(u32_t event, u32_t param)
{
	send_event_inner(event, param, 0, true);
}

static void send_short(u32_t event)
{
	u32_t time = get_time();
	send_event_inner(event, 0, time, false);
}

static void send_thread_info(k_tid_t thread)
{
	u32_t size = thread->stack_info.size;
	u32_t start = thread->stack_info.start;
	const u8_t *name = (const u8_t *)k_thread_name_get(thread);

	send_timeless(EV_THREAD_INFO_BEGIN | (size & 0xFFFF), (u32_t)thread);
	send_timeless(EV_THREAD_INFO_NEXT | (size >> 16), (u32_t)thread);
	send_timeless(EV_THREAD_INFO_NEXT | (start & 0xFFFF), (u32_t)thread);
	if (name != NULL && name[0] != 0) {
		u32_t pair;
		send_timeless(EV_THREAD_INFO_NEXT | (start >> 16), (u32_t)thread);
		do {
			pair = (u32_t)name[0] | ((u32_t)name[1] << 8);
			if (name[1] == 0 || name[2] == 0) {
				break;
			}
			send_timeless(EV_THREAD_INFO_NEXT | pair, (u32_t)thread);
			name += 2;
		} while (true);
		send_timeless(EV_THREAD_INFO_END | pair, (u32_t)thread);
	} else {
		send_timeless(EV_THREAD_INFO_END | (start >> 16), (u32_t)thread);
	}
}

static void send_periodic_thread_info()
{
	static struct k_thread *next_thread = NULL;
	struct k_thread *thread = NULL;
	for (thread = _kernel.threads; thread; thread = thread->next_thread) {
		if (thread == next_thread) {
			break;
		}
	}
	if (!thread)
	{
		thread = _kernel.threads;
	}
	next_thread = thread->next_thread;
	send_thread_info(thread);
}

static void initialize()
{
	static bool initialized = false;

	if (!initialized)
	{
		nrfx_timer_config_t timer_conf = NRFX_TIMER_DEFAULT_CONFIG;

		/*
		 * Directly initialize RTT up channel to avoid unexpected traces
		 * before initialization.
		 */
		SEGGER_RTT_BUFFER_UP *up;
		up = &_SEGGER_RTT.aUp[CONFIG_RTT_LITE_TRACE_RTT_CHANNEL];
		up->sName = CHANNEL_NAME;
		up->pBuffer = (char*)(&rtt_buffer[0]);
		up->SizeOfBuffer = sizeof(rtt_buffer);
		up->RdOff = 0u;
		up->WrOff = 0u;
		up->Flags = SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL;

		RTT_BUFFER_U32(RTT_BUFFER_BYTES) = EV_BUFFER_CYCLE;
		if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_FAST_OVERFLOW_CHECK)) {
			RTT_BUFFER_U32(RTT_BUFFER_BYTES + 4) = 1;
		} else {
			RTT_BUFFER_U32(RTT_BUFFER_BYTES + 4) = RTT_BUFFER_BYTES;
		}

		timer_conf.frequency = NRF_TIMER_FREQ_16MHz;
		timer_conf.bit_width = NRF_TIMER_BIT_WIDTH_24;

		nrfx_timer_init(&timer, &timer_conf, NULL);
		nrfx_timer_enable(&timer);
		
		send_short(EV_SYSTEM_RESET);

		initialized = true;
	}
}

void sys_trace_thread_switched_in(void)
{
	k_tid_t thread = k_current_get();
	if (z_is_idle_thread_object(thread)) {
		send_event(EV_IDLE, RTT_BUFFER_U32(RTT_BUFFER_BYTES + 4));
	} else {
		send_event(EV_THREAD_START, (u32_t)thread);
	}
}

void sys_trace_thread_switched_out(void)
{
	send_short(EV_THREAD_STOP);
}

void sys_trace_isr_enter(void)
{
	if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_IRQ)) {
		u32_t num = get_isr_number();
		send_short(EV_ISR_ENTER | (num << 24));
	}
}

void sys_trace_isr_exit(void)
{
	if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_IRQ)) {
		send_short(EV_ISR_EXIT);
	}
}

void sys_trace_idle(void)
{
	send_event(EV_IDLE, RTT_BUFFER_U32(RTT_BUFFER_BYTES + 4));
	if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_THREAD_INFO))
	{
		send_periodic_thread_info();
	}
}

void sys_trace_thread_priority_set(k_tid_t thread)
{
	u8_t prio = (u8_t)thread->base.prio;
	send_timeless(EV_THREAD_PRIORITY | (u32_t)prio, (u32_t)thread);
}

void sys_trace_thread_create(k_tid_t thread)
{
	initialize();
	send_event(EV_THREAD_CREATE, (u32_t)thread);
	sys_trace_thread_priority_set(thread);
	if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_THREAD_INFO))
	{
		send_thread_info(thread);
	}
}

void sys_trace_thread_suspend(k_tid_t thread)
{
	send_event(EV_THREAD_SUSPEND, (u32_t)thread);
}

void sys_trace_thread_resume(k_tid_t thread)
{
	send_event(EV_THREAD_RESUME, (u32_t)thread);
}

void sys_trace_thread_ready(k_tid_t thread)
{
	send_event(EV_THREAD_READY, (u32_t)thread);
}

void sys_trace_thread_pend(k_tid_t thread)
{
	send_event(EV_THREAD_PEND, (u32_t)thread);
}

#ifdef CONFIG_RTT_LITE_TRACE_THREAD_INFO

void sys_trace_thread_name_set(k_tid_t thread)
{
	send_thread_info(thread);
}

#endif /* CONFIG_RTT_LITE_TRACE_THREAD_INFO */

#ifdef CONFIG_RTT_LITE_TRACE_SYNCHRO

void sys_trace_void(u32_t id)
{
	send_event(EV_SYS_CALL, id);
}

void sys_trace_end_call(u32_t id)
{
	send_event(EV_SYS_END_CALL, id);
}

#endif /* CONFIG_RTT_LITE_TRACE_SYNCHRO */
