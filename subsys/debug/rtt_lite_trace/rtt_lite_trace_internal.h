/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef _RTT_LITE_TRACE_INTERNAL_H_
#define _RTT_LITE_TRACE_INTERNAL_H_

/* 
 * Events are grouped. Following table summarizes the groups.
 *
 * +--------------------------------+------------------------------------------+
 * | First event number of a group  | Description                              |
 * +--------------------------------+------------------------------------------+
 * | EV_NUMBER_INVALID              | Invalid event number (i.e. zero).        |
 * +--------------------------------+------------------------------------------+
 * | EV_NUMBER_FIRST_WITH_ADD       | Events with 24-bit additional parameter  |
 * |                                | and no timestamp.                        |
 * +--------------------------------+------------------------------------------+
 * | EV_NUMBER_FIRST_WITH_TIMESTAMP | Events with 24-bit timestamp.            |
 * +--------------------------------+------------------------------------------+
 * | EV_NUMBER_FIRST_USER           | Events with 24-bit timestamp that can be |
 * |                                | used by the user.                        |
 * +--------------------------------+------------------------------------------+
 * | EV_NUMBER_FIRST_RESERVED       | Event numbers reserved for byte          |
 * |                                | synchronization.                         |
 * +--------------------------------+------------------------------------------+
 * | EV_NUMBER_FIRST_WITH_ISR       | Events with 7-bit ISR number and 24-bit  |
 * |                                | timestamp synchronization.               |
 * +--------------------------------+------------------------------------------+
 * 
 * Each event also contains one 32-bit parameter.
 */

/** @brief Event number 0 is reserved and should be interpreted as invalid
 */
#define EV_NUMBER_INVALID 0x00000000

/* ========================================================================== */

/** @brief Event number that starts group of events with 24-bit additional
 * parameter and no timestamp.
 *
 * Event format:
 *     0eee eeee pppp pppp pppp pppp pppp pppp
 *     e - event numer
 *     p - additional parameter
 */
#define EV_NUMBER_FIRST_WITH_ADD 0x01000000

/** @brief Event send periodically to allow synchronization of the stream.
 * 
 * Each byte of the event is not a valid event id, so it gives the hint to the
 * parser that this is a synchronization event.
 * 
 * @param additional  Always SYNC_ADDITIONAL.
 * @param param       Always SYNC_PARAM.
 */
#define EV_SYNC 0x01000000

/** @brief Event indicating cycle of RTT buffer.
 *
 * This event is placed at the end of RTT buffer, so it is send each time
 * RTT buffer cycles back to the beginning.
 * @param additional Protocol version
 * @param param      If CONFIG_RTT_LITE_TRACE_FAST_OVERFLOW_CHECK is set then
 *         it contains 1 on bit 0 and RTT buffer cycle counter on the rest of
 *         bits. Else it contains minimum free space of RTT buffer if
 *         CONFIG_RTT_LITE_TRACE_BUFFER_STATS is set.
 */
#define EV_CYCLE 0x02000000

/** @brief Event containing thread information.
 * 
 * Buffer containing additional information is send before this event
 * if bit 17 or above is not zero.
 * 
 * +---------+----------------------------------------------------+
 * | Size    | Description                                        |
 * | (bytes) |                                                    |
 * +=========+====================================================+
 * | 0 or 4  | Stack size.                                        |
 * +---------+----------------------------------------------------+
 * | 0 or 4  | Stack base address.                                |
 * +---------+----------------------------------------------------+
 * | 0..n    | Thread name string. It is NOT null terminated.     |
 * +---------+----------------------------------------------------+
 *
 * @param additional Bits 0..15 - thread priority,
 * 		     Bit     16 - set if this is an idle thread,
 * 		     Bit     17 - set if the stack info is provided,
 * 		     Bit     18 - set if the name is provided.
 * @param param      Thread id.
 */
#define EV_THREAD_INFO 0x03000000

/** @brief Sends format information for formatted text output.
 * 
 * Buffer containing format string is send immediately before this event.
 * It is not null-terminated.
 * 
 * @param additional Unused.
 * @param param      Format id. See EV_PRINTF for details.
 */
#define EV_FORMAT 0x04000000

/** @brief Start sending buffer.
 * 
 * Buffers are send immediately before specific events to provide more data.
 * In the same time two thread may send buffers, so the receiving part have to
 * assemble buffer back separately for each thread.
 * 
 * @param additional Next 3 bytes of the buffer.
 * @param param      First 4 bytes of the buffer.
 */
#define EV_BUFFER_BEGIN 0x05000000

/** @brief Continue sending buffer.
 * 
 * @param additional Next 3 bytes of the buffer that follows bytes from param.
 * @param param      Next 4 bytes of the buffer.
 */
#define EV_BUFFER_NEXT 0x06000000

/** @brief Continue sending buffer.
 * 
 * @param additional Next 2 bytes of the buffer that follows bytes from param.
 *         Byte 3 contains number of buffer bytes that are actually in this
 *         event, because last part of the buffer may not fill this event
 *         entirety.
 * @param param      Next 4 bytes of the buffer.
 */
#define EV_BUFFER_END 0x07000000

/** @brief Send small buffer in one pice.
 * 
 * This event is send when the buffer is smaller than 7 bytes.
 *
 * @param additional Next 2 bytes of the buffer.
 *         Byte 3 contains number of buffer bytes that are actually in this
 *         event, because the buffer may not fill this event entirety.
 * @param param      First 4 bytes of the buffer.
 */
#define EV_BUFFER_BEGIN_END 0x08000000

/** @brief Event that gives a name of a resource for pritty printing.
 * 
 * Buffer containing resource name is send immediately before this event.
 * 
 * @param additional  Unused.
 * @param param       Resource id which is usually pointer to it.
 */
#define EV_RES_NAME 0x09000000

/* ========================================================================== */

/** @brief Event number that starts group of events with 24-bit timestamp.
 *
 * Event format:
 *     0eee eeee tttt tttt tttt tttt tttt tttt
 *     e - event numer
 *     t - time stamp
 */
#define EV_NUMBER_FIRST_WITH_TIMESTAMP 0x0A000000

/** @brief Event send if RTT buffer cannot fit next event.
 * 
 * @param param      Number of events that were skipped because of overflow.
 */
#define EV_OVERFLOW 0x0A000000

/** @brief Event send when systems goes into idle.
 * 
 * @param param       The same as EV_CYCLE.
 */
#define EV_IDLE 0x0B000000

/** @brief Event send when systems starts executing a thread.
 * 
 * @param param       Thread id.
 */
#define EV_THREAD_START 0x0C000000

/** @brief Event send when systems stops executing a thread and goes to
 *  the scheduler.
 * 
 * @param param       Thread id.
 */
#define EV_THREAD_STOP 0x0D000000

/** @brief Event send when systems creates a new thread.
 * 
 * @param param       New thread id.
 */
#define EV_THREAD_CREATE 0x0E000000

/** @brief Event send when a thread was suspended.
 * 
 * @param param       New thread id.
 */
#define EV_THREAD_SUSPEND 0x0F000000

/** @brief Event send when a thread was resumed.
 * 
 * @param param       New thread id.
 */
#define EV_THREAD_RESUME 0x10000000

/** @brief Event send when a thread is ready to be executed.
 * 
 * @param param       New thread id.
 */
#define EV_THREAD_READY 0x11000000

/** @brief Event send when a thread is peding.
 * 
 * @param param       New thread id.
 */
#define EV_THREAD_PEND 0x12000000

/** @brief Event send when a specific system function was called.
 * 
 * @param param       Function id, see SYS_TRACE_ID_xyz for full list.
 */
#define EV_SYS_CALL 0x13000000

/** @brief Event send when a specific system function returned.
 * 
 * @param param       Function id, see SYS_TRACE_ID_xyz for full list.
 */
#define EV_SYS_END_CALL 0x14000000

/** @brief Event send when currently running ISR exits.
 * 
 * @param param       Unused.
 */
#define EV_ISR_EXIT 0x15000000

/** @brief Event send when currently running ISR exits to the scheduler.
 * 
 * @param param       Unused.
 */
#define EV_ISR_EXIT_TO_SCHEDULER 0x16000000

/** @brief Event send to print when formated text.
 * 
 * Buffer is send immediately before this event if bit 31 of param is cleared.
 * If format id is 0xFFFFFF it contais null terminated format string. After that
 * it contains arguments according to arguments list:
 *     * FORMAT_ARG_INT32 - 4 byte integer,
 *     * FORMAT_ARG_INT64 - 8 byte integer,
 *     * FORMAT_ARG_STRING - null terminated text string.
 *
 * Format id is following:
 *	Bits  0:23 - Unique number of the format string or 0xFFFFFF to indicate
 *		     that format string was added to the beginning of the
 *		     buffer.
 *      Bits 24:30 - Level of the message, see RTT_LITE_TRACE_LEVEL_xyz.
 *	Bit     31 - If set, indicates that before this event buffer was send.
 *		     Buffer may be skipped if format string was send before with
 *		     EV_FORMAT event and it does not contains any arguments.
 *
 * @param param   Format id.
 */
#define EV_PRINTF 0x17000000

/** @brief Event send to print a string.
 * 
 * Buffer is send immediately before this event containing the string
 * (without null terminator).
 * 
 * @param param Level of the message, see RTT_LITE_TRACE_LEVEL_xyz.
 */
#define EV_PRINT 0x18000000

/** @brief Event indicating start of marker.
 * 
 * @param param Marker id.
 */
#define EV_MARK_START 0x19000000

/** @brief Event indicating marker.
 * 
 * @param param Marker id.
 */
#define EV_MARK 0x1A000000

/** @brief Event indicating end of marker.
 * 
 * @param param Marker id.
 */
#define EV_MARK_STOP 0x1B000000

/** @brief Event send as the first event after the system reset.
 * 
 * @param param      Version of the protocol. Currently only 0.
 */
#define EV_SYSTEM_RESET 0x1C000000

/* ========================================================================== */

/** @brief First event number that can be used by the user.
 */
#define EV_NUMBER_FIRST_USER 0x1D000000

/* ========================================================================== */

/** @brief First event number that is reserved for synchronization.
 */
#define EV_NUMBER_FIRST_RESERVED 0x7C000000

/* ========================================================================== */

/** @brief Event number that starts group of events with 7-bit ISR number and
 * 24-bit timestamp.
 *
 * Event format:
 *     eiii iiii tttt tttt tttt tttt tttt tttt
 *     e - event numer
 *     i - ISR number
 *     t - time stamp
 */
/** @brief First event number with 
 */
#define EV_NUMBER_FIRST_WITH_ISR 0x80000000

/** @brief Event send when ISR was started.
 * 
 * @param isr       Number of the ISR that was started.
 * @param param     Unused.
 */
#define EV_ISR_ENTER 0x80000000

/* ========================================================================== */

/** @brief Protocol version. It will change if something in events format will
 * change.
 */
#define PROTOCOL_VERSION 0

/** @brief Parameter for EV_SYNC.
 * 
 * Each byte of this parameter (and additional) is not valid event id, so if
 * byte missalignment will happen it will be detected at this event.
 */
#define SYNC_PARAM 0x7F7D7E7C

/** @brief Additional parameter for EV_SYNC.
 */
#define SYNC_ADDITIONAL 0x007F7E7D

/** @brief Flag for EV_THREAD_INFO indicating that thread is an idle thread.
 */
#define THREAD_INFO_IDLE          (1 << 16)

/** @brief Flag for EV_THREAD_INFO indicating that buffer is present and there
 * is stack information in it.
 */
#define THREAD_INFO_STACK_PRESENT (1 << 17)

/** @brief Flag for EV_THREAD_INFO indicating that buffer is present and there
 * is a thread name in it.
 */
#define THREAD_INFO_NAME_PRESENT  (1 << 18)

/** @brief RTT channel name used to identify trace channel.
 */
#define CHANNEL_NAME "NrfLiteTrace"

#endif /* _RTT_LITE_TRACE_INTERNAL_H_ */
