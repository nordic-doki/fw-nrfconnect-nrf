/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file
 *  @brief Nordic UART Bridge Service (NUS) sample
 */

#include <drivers/uart.h>
#include <zephyr/types.h>
#include <zephyr.h>

#include <device.h>
#include <soc.h>

#include <dk_buttons_and_leds.h>

#include <stdio.h>
#include <string.h>
#include <init.h>

#include "bt_ser.h"
#include "rpmsg.h" 

#include "debug/tracing.h"

#define DISABLE_UART
#define DISABLE_BLINKY
#define ENABLE_CPU_STATS

#define STACKSIZE CONFIG_BT_GATT_NUS_THREAD_STACK_SIZE
#define PRIORITY 7

#define RUN_STATUS_LED DK_LED1
#define RUN_LED_BLINK_INTERVAL 1000

#define CON_STATUS_LED DK_LED2

#define UART_BUF_SIZE CONFIG_BT_GATT_NUS_UART_BUFFER_SIZE

#define BT_ADDR_LE_STR_LEN 30

static K_SEM_DEFINE(ble_init_ok, 0, 2);

#if !defined(DISABLE_UART)

static struct device *uart;

struct uart_data_t {
	void *fifo_reserved;
	u8_t data[UART_BUF_SIZE];
	u16_t len;
};

static K_FIFO_DEFINE(fifo_uart_tx_data);
static K_FIFO_DEFINE(fifo_uart_rx_data);

#endif

#if defined(ENABLE_CPU_STATS)

static struct k_delayed_work cpu_marker_show;

static void cpu_stats_marker(const char* text)
{
	struct cpu_stats stats;
	cpu_stats_get_ns(&stats);
	uint64_t ppm = (stats.non_idle + stats.sched) * (uint64_t)1000000 / (stats.non_idle + stats.sched + stats.idle);
	printk("\n~ %10u %10u %10u  %3u.%03u %s\n", (uint32_t)((stats.non_idle + (uint64_t)500) / (uint64_t)1000),
		(uint32_t)((stats.sched + (uint64_t)500) / (uint64_t)1000),
		(uint32_t)((stats.idle + (uint64_t)500) / (uint64_t)1000),
		(uint32_t)ppm / 1000,
		(uint32_t)ppm % 1000,
		text);
	k_delayed_work_cancel(&cpu_marker_show);
	cpu_stats_reset_counters();
}

static void show_delayed_cpu_stats(struct k_work *work)
{
	cpu_stats_marker("Delayed");
}

#endif

static int bt_addr_le_to_str(const bt_addr_le_t *addr, char *str,
			     size_t len)
{
	char type[10];

	switch (addr->type) {
	case BT_ADDR_LE_PUBLIC:
		strcpy(type, "public");
		break;
	case BT_ADDR_LE_RANDOM:
		strcpy(type, "random");
		break;
	case BT_ADDR_LE_PUBLIC_ID:
		strcpy(type, "public-id");
		break;
	case BT_ADDR_LE_RANDOM_ID:
		strcpy(type, "random-id");
		break;
	default:
		snprintk(type, sizeof(type), "0x%02x", addr->type);
		break;
	}

	return snprintk(str, len, "%02X:%02X:%02X:%02X:%02X:%02X (%s)",
			addr->a.val[5], addr->a.val[4], addr->a.val[3],
			addr->a.val[2], addr->a.val[1], addr->a.val[0], type);
}

#if !defined(DISABLE_UART)

static void uart_cb(struct device *uart)
{
	static struct uart_data_t *rx;

	uart_irq_update(uart);

	if (uart_irq_rx_ready(uart)) {
		int data_length;

		if (!rx) {
			rx = k_malloc(sizeof(*rx));
			if (rx) {
				rx->len = 0;
			} else {
				char dummy;

				printk("Not able to allocate UART receive buffer\n");

				/* Drop one byte to avoid spinning in a
				 * eternal loop.
				 */
				uart_fifo_read(uart, &dummy, 1);

				return;
			}
		}

		data_length = uart_fifo_read(uart, &rx->data[rx->len],
		UART_BUF_SIZE - rx->len);
		rx->len += data_length;

		if (rx->len > 0) {
			/* Send buffer to bluetooth unit if either buffer size
			 * is reached or the char \n or \r is received, which
			 * ever comes first
			 */
			if ((rx->len == UART_BUF_SIZE) ||
			    (rx->data[rx->len - 1] == '\n') ||
			    (rx->data[rx->len - 1] == '\r')) {
				k_fifo_put(&fifo_uart_rx_data, rx);
				rx = NULL;
			}
		}
	}

	if (uart_irq_tx_ready(uart)) {
		struct uart_data_t *buf = k_fifo_get(&fifo_uart_tx_data,
						     K_NO_WAIT);
		u16_t written = 0;

		/* Nothing in the FIFO, nothing to send */
		if (!buf) {
			uart_irq_tx_disable(uart);
			return;
		}

		while (buf->len > written) {
			written += uart_fifo_fill(uart, &buf->data[written],
					buf->len - written);
		}

		while (!uart_irq_tx_complete(uart)) {
			/* Wait for the last byte to get
			 * shifted out of the module
			 */
		}

		if (k_fifo_is_empty(&fifo_uart_tx_data)) {
			uart_irq_tx_disable(uart);
		}

		k_free(buf);
	}
}

static int uart_init(void)
{
	uart = device_get_binding("UART_0");
	if (!uart) {
		return -ENXIO;
	}

	uart_irq_callback_set(uart, uart_cb);
	uart_irq_rx_enable(uart);

	return 0;
}

#endif

static void bt_connected(const bt_addr_le_t *addr, u8_t err)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	if (err) {
		printk("Connection failed (err %u)\n", err);
		return;
	}

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

	printk("Connected %s\n", addr_str);

	dk_set_led_on(CON_STATUS_LED);
}

static void bt_disconnected(const bt_addr_le_t *addr, u8_t reason)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

	printk("Disconnected: %s (reason %u)\n", addr_str, reason);

	dk_set_led_off(CON_STATUS_LED);
}

static void bt_received(const bt_addr_le_t *addr, const u8_t *data, size_t len)
{
#if !defined(DISABLE_UART)

	char addr_str[BT_ADDR_LE_STR_LEN] = { 0 };

	bt_addr_le_to_str(addr, addr_str, ARRAY_SIZE(addr_str));

	printk("Received data from: %s\n", addr_str);

	for (u16_t pos = 0; pos != len;) {
		struct uart_data_t *tx = k_malloc(sizeof(*tx));

		if (!tx) {
			printk("Not able to allocate UART send data buffer\n");
			return;
		}

		/* Keep the last byte of TX buffer for potential LF char. */
		size_t tx_data_size = sizeof(tx->data) - 1;

		if ((len - pos) > tx_data_size) {
			tx->len = tx_data_size;
		} else {
			tx->len = (len - pos);
		}

		memcpy(tx->data, &data[pos], tx->len);

		pos += tx->len;

		/* Append the LF character when the CR character triggered
		 * transmission from the peer.
		 */
		if ((pos == len) && (data[len - 1] == '\r')) {
			tx->data[tx->len] = '\n';
			tx->len++;
		}

		k_fifo_put(&fifo_uart_tx_data, tx);
	}

	/* Start the UART transfer by enabling the TX ready interrupt */
	uart_irq_tx_enable(uart);
#else

	static char buf[3] = " \r\n";
	static char bit = 0;
	buf[0] = ('@' + len) ^ bit;
	bit ^= 32;
	int err = bt_nus_transmit(buf, 3);
	if (err) 
	{
		printk("bt_nus_transmit error: %d\n", err);
	}

#endif
}

static const struct bt_nus_cb bt_nus_callbacks = {
	.bt_connected    = bt_connected,
	.bt_disconnected = bt_disconnected,
	.bt_received = bt_received,
};

void error(void)
{
	dk_set_leds_state(DK_ALL_LEDS_MSK, DK_NO_LEDS_MSK);

	while (true) {
		/* Spin for ever */
		k_sleep(1000);
	}
}

#if defined(ENABLE_CPU_STATS)

static void button_changed(u32_t button_state, u32_t has_changed)
{
	u32_t buttons = button_state & has_changed;
	if (buttons & DK_BTN3_MSK) {
		cpu_stats_marker("Button + 60sec");
		k_delayed_work_submit(&cpu_marker_show, K_SECONDS(60));
		cpu_stats_reset_counters();
	} else if (buttons & DK_BTN4_MSK) {
		cpu_stats_marker("Button + 20sec");
		k_delayed_work_submit(&cpu_marker_show, K_SECONDS(20));
		cpu_stats_reset_counters();
	}
}

#endif

static void configure_gpio(void)
{
	int err;

	err = dk_leds_init();
	if (err) {
		printk("Cannot init LEDs (err: %d)\n", err);
	}

#if defined(ENABLE_CPU_STATS)

	err = dk_buttons_init(button_changed);
	if (err) {
		printk("Cannot init buttons (err: %d)\n", err);
	}

	k_delayed_work_init(&cpu_marker_show, show_delayed_cpu_stats);

#endif
}

static void led_blink_thread(void)
{
#if !defined(DISABLE_BLINKY)
	int blink_status = 0;
#endif
	int err = 0;

	ipc_register_rx_callback(bt_nus_rx_parse);
	err = ipc_init();
	if (err) {
		printk("Rpmsg init error %d\n", err);
	}

#if !defined(DISABLE_UART)
	err = uart_init();
	if (err) {
		error();
	}
#endif

	configure_gpio();

	bt_nus_callback_register(&bt_nus_callbacks);

	err = bt_nus_init();
	if (err < 0) {
		printk("NUS service initialization failed\n");
		error();
	}

	k_sem_give(&ble_init_ok);

	printk("Starting Nordic UART service example[APP CORE]\n");

#if !defined(DISABLE_BLINKY)
	for (;;) {
		dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
		k_sleep(RUN_LED_BLINK_INTERVAL);
	}
#endif
}

#if !defined(DISABLE_UART)

void ble_write_thread(void)
{
	int err = 0;
	/* Don't go any further until BLE is initailized */
	k_sem_take(&ble_init_ok, K_FOREVER);

	for (;;) {
		/* Wait indefinitely for data to be sent over bluetooth */
		struct uart_data_t *buf = k_fifo_get(&fifo_uart_rx_data,
						     K_FOREVER);

		err = bt_nus_transmit(buf->data, buf->len);
		printk("NUS send %d bytes status %d\n", buf->len, err);

		k_free(buf);
	}
}

K_THREAD_DEFINE(ble_write_thread_id, STACKSIZE, ble_write_thread, NULL, NULL,
		NULL, PRIORITY, 0, K_NO_WAIT);

#endif

K_THREAD_DEFINE(led_blink_thread_id, STACKSIZE, led_blink_thread, NULL, NULL,
		NULL, PRIORITY, 0, K_NO_WAIT);
