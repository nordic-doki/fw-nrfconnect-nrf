/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SERIALIZATION_H_
#define SERIALIZATION_H_

#include <bluetooth/addr.h>

/**
 * @file
 * @defgroup bt_serialization BLE Nordic UART Service serialization
 * @{
 * @brief BLE Nordic UART(NUS) serialization API.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**@brief Callback structure used bt the BT NUS serialization. */
struct bt_nus_cb {
	/**@brief A new connection has been established on the Network core.
	 *
	 * This callback inform the Application core about a new
	 * connection.
	 *
	 * @param[in] addr Address of connected device.
	 * @param[in] err HCI error. Zero for success, non-zero otherwise.
	 */
	void (*bt_connected)(const bt_addr_le_t *addr, u8_t err);

	/**@brief A connection has been terminated on the Remote core.
	 *
	 * This callback notifies an Application core that a
	 * connection has been disconnected.
	 *
	 * @param[in] addr Address of connected device.
	 * @param[in] reason HCI reason for the disconnection.
	 */
	void (*bt_disconnected)(const bt_addr_le_t *addr, u8_t reason);

	/**@brief A new NUS data has been received on the remote core.
	 *
	 * This callback notifies a application processor that the Nordic\
	 * UART Service received new data.
	 *
	 * @param[in] addr Device address which sent data.
	 * @param[in] data Received NUS data.
	 * @param[in] lem data length.
	 */
	void (*bt_received)(const bt_addr_le_t *addr, const u8_t *data,
			    size_t len);
};

/**@brief Register bluetooth application callback.
 *
 * @param[in] cb Callback structure
 */
void bt_nus_callback_register(const struct bt_nus_cb *cb);

/**@brief Initialize Bluetooth NUS service on the Network
 *        core.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_nus_init(void);

/**@brief Send received Nordic UART Service (NUS) to
 *        application processor.
 *
 * This function sends data to remote core and
 * wait a specific amount of time for remote function
 * call return value. On remote processor function
 * for sending data over NUS is called.
 *
 * @param[in] data NUS data to send.
 * @param[in] length Data length.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_nus_transmit(const u8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

/**
 *@}
 */

#endif /* SERIALIZATION_H_ */