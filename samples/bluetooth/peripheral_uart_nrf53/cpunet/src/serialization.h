/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SERIALIZATION_H_
#define SERIALIZATION_H_

#include <errno.h>
#include <cbor.h>
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

/**@brief Notify an Application core about a new connection.
 *
 * @param[in] addr Address of connected device.
 * @param[in] err HCI error. Zero for success, non-zero otherwise.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_nus_connection_evt_send(const bt_addr_le_t *addr, u8_t error);

/**@brief Notify an Application core that a connection
 *        has been disconnected.
 *
 * @param[in] addr Address of connected device.
 * @param[in] err HCI error. Zero for success, non-zero otherwise.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_nus_disconnection_evt_send(const bt_addr_le_t *addr, u8_t reason);

/**@brief Notify an Application core that the NUS service
 *        received new data.
 *
 * @param[in] addr Device address which sent data.
 * @param[in] data Received data.
 * @param[in] length Data length.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_nus_received_evt_send(const bt_addr_le_t *addr,
			     const u8_t *data, size_t length);

#ifdef __cplusplus
}
#endif



/**
 *@}
 */

#endif /* SERIALIZATION_H_ */