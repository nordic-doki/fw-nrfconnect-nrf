/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _RTT_LIGHT_TRACE_H
#define _RTT_LIGHT_TRACE_H
#include <kernel.h>
#include <kernel_structs.h>
#include <init.h>

#define RTT_LITE_TRACE_USER_EVENT_FIRST 0x30000000
#define RTT_LITE_TRACE_USER_EVENT_STEP 0x01000000
#define RTT_LITE_TRACE_USER_EVENT_LAST 0x6F000000
#define RTT_LITE_TRACE_USER_TIMELESS_EVENT_FIRST 0x00100000
#define RTT_LITE_TRACE_USER_TIMELESS_EVENT_STEP 0x00010000
#define RTT_LITE_TRACE_USER_TIMELESS_EVENT_LAST 0x007F0000

#define RTT_LITE_TRACE_USER_EVENT(id) \
		((id) * RTT_LITE_TRACE_USER_EVENT_STEP \
		+ RTT_LITE_TRACE_USER_EVENT_FIRST)
#define RTT_LITE_TRACE_USER_TIMELESS_EVENT(id) \
		((id) * RTT_LITE_TRACE_USER_TIMELESS_EVENT_STEP \
		+ RTT_LITE_TRACE_USER_TIMELESS_EVENT_FIRST)

#define RTT_LITE_TRACE_LEVEL_LOG 0
#define RTT_LITE_TRACE_LEVEL_WARNING 1
#define RTT_LITE_TRACE_LEVEL_ERROR 2

#define RTT_LITE_TRACE_FORMAT_INIT(_level, format_string) { \
	.text = (format_string), .id = 0, .level = (_level) .args = { 0 } }

#define RTT_LITE_TRACE_PRINTF(level, format_string, ...) \
	do { \
		static struct rtt_lite_trace_format _lite_trace_fmt = \
			RTT_LITE_TRACE_FORMAT_INIT(level, format_string); \
		rtt_lite_trace_printf(&_lite_trace_fmt, ##__VA_ARGS__); \
	} while (0);

#define RTT_LITE_TRACE_LOGF(format, ...) \
	RTT_LITE_TRACE_PRINTF(RTT_LITE_TRACE_LEVEL_LOG, format, ##__VA_ARGS__)
#define RTT_LITE_TRACE_WARNF(format, ...) \
	RTT_LITE_TRACE_PRINTF(RTT_LITE_TRACE_LEVEL_WARNING, format, ##__VA_ARGS__)
#define RTT_LITE_TRACE_ERRF(format, ...) \
	RTT_LITE_TRACE_PRINTF(RTT_LITE_TRACE_LEVEL_ERROR, format, ##__VA_ARGS__)
#define RTT_LITE_TRACE_LOG(text)) \
	rtt_lite_trace_print(RTT_LITE_TRACE_LEVEL_LOG, (text))
#define RTT_LITE_TRACE_WARN(text)) \
	rtt_lite_trace_print(RTT_LITE_TRACE_LEVEL_WARNING, (text))
#define RTT_LITE_TRACE_ERR(text)) \
	rtt_lite_trace_print(RTT_LITE_TRACE_LEVEL_ERROR, (text))

struct rtt_lite_trace_format
{
	const char* text;
	u32_t id;
	u8_t level;
	u8_t args[CONFIG_RTT_LITE_TRACE_PRINTF_MAX_ARGS + 1];
};

void rtt_lite_trace_event(u32_t event, u32_t param);
void rtt_lite_trace_event_timeless(u32_t event, u32_t param32, u16_t param16);
void rtt_lite_trace_print(u32_t level, const char *text);
void rtt_lite_trace_printf(struct rtt_lite_trace_format *format, ...);

u32_t rtt_lite_trace_time();

void rtt_lite_trace_mark_start(u32_t mark_id);
void rtt_lite_trace_mark(u32_t mark_id);
void rtt_lite_trace_mark_stop(u32_t mark_id);

void rtt_lite_trace_call(u32_t event);
void rtt_lite_trace_call_1(u32_t event, u32_t arg1);
void rtt_lite_trace_call_v(u32_t event, u32_t num_args, u32_t arg1, ...);

void rtt_lite_trace_name(u32_t resource_id, const char* name);

void sys_trace_thread_switched_in(void);
void sys_trace_thread_switched_out(void);
void sys_trace_isr_enter(void);
void sys_trace_isr_exit(void);
void sys_trace_idle(void);

void sys_trace_thread_priority_set(k_tid_t thread);
void sys_trace_thread_create(k_tid_t thread);
void sys_trace_thread_suspend(k_tid_t thread);
void sys_trace_thread_resume(k_tid_t thread);
void sys_trace_thread_ready(k_tid_t thread);
void sys_trace_thread_pend(k_tid_t thread);
#ifdef CONFIG_RTT_LITE_TRACE_THREAD_INFO
void sys_trace_thread_name_set(k_tid_t thread);
#else
#define sys_trace_thread_name_set(thread)
#endif
#ifdef CONFIG_RTT_LITE_TRACE_SYNCHRO
void sys_trace_void(u32_t id);
void sys_trace_end_call(u32_t id);
#else
#define sys_trace_void(id)
#define sys_trace_end_call(id)
#endif

// Trace macros that are ignored
#define sys_trace_thread_abort(thread)

// Trace macros that are actually never called by the Zephyr kernel
#define sys_trace_isr_exit_to_scheduler() 
#define sys_trace_thread_info(thread)

#endif /* _RTT_LIGHT_TRACE_H */
