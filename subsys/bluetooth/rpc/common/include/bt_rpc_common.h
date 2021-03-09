/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @defgroup bt_rpc_common Bluetooth RPC common API
 * @{
 * @brief Common API for the Bluetooth RPC.
 */

#ifndef BT_RPC_COMMON_H_
#define BT_RPC_COMMON_H_

#include "bluetooth/bluetooth.h"
#include "bluetooth/conn.h"

#include "nrf_rpc_cbor.h"

/** @brief Client commands IDs used in bluetooth API serialization.
 *         Those commands are sent from the client to the host.
 */
enum bt_rpc_cmd_from_cli_to_host
{
	/* bluetooth.h API */
	BT_RPC_GET_CHECK_LIST_RPC_CMD,
	BT_ENABLE_RPC_CMD,
	BT_LE_ADV_START_RPC_CMD,
	BT_LE_ADV_STOP_RPC_CMD,
	BT_LE_SCAN_START_RPC_CMD,
	BT_SET_NAME_RPC_CMD,
	BT_GET_NAME_OUT_RPC_CMD,
	BT_SET_ID_ADDR_RPC_CMD,
	BT_ID_GET_RPC_CMD,
	BT_ID_CREATE_RPC_CMD,
	BT_ID_RESET_RPC_CMD,
	BT_ID_DELETE_RPC_CMD,
	BT_LE_ADV_UPDATE_DATA_RPC_CMD,
	BT_LE_EXT_ADV_CREATE_RPC_CMD,
	BT_LE_EXT_ADV_DELETE_RPC_CMD,
	BT_LE_EXT_ADV_START_RPC_CMD,
	BT_LE_EXT_ADV_STOP_RPC_CMD,
	BT_LE_EXT_ADV_SET_DATA_RPC_CMD,
	BT_LE_EXT_ADV_UPDATE_PARAM_RPC_CMD,
	BT_LE_EXT_ADV_GET_INDEX_RPC_CMD,
	BT_LE_EXT_ADV_GET_INFO_RPC_CMD,
	BT_LE_PER_ADV_SET_PARAM_RPC_CMD,
	BT_LE_PER_ADV_SET_DATA_RPC_CMD,
	BT_LE_PER_ADV_START_RPC_CMD,
	BT_LE_PER_ADV_STOP_RPC_CMD,
	BT_LE_PER_ADV_SYNC_GET_INDEX_RPC_CMD,
	BT_LE_PER_ADV_SYNC_CREATE_RPC_CMD,
	BT_LE_PER_ADV_SYNC_DELETE_RPC_CMD,
	BT_LE_PER_ADV_SYNC_CB_REGISTER_ON_REMOTE_RPC_CMD,
	BT_LE_PER_ADV_SYNC_RECV_ENABLE_RPC_CMD,
	BT_LE_PER_ADV_SYNC_RECV_DISABLE_RPC_CMD,
	BT_LE_PER_ADV_SYNC_TRANSFER_RPC_CMD,
	BT_LE_PER_ADV_SET_INFO_TRANSFER_RPC_CMD,
	BT_LE_PER_ADV_SYNC_TRANSFER_SUBSCRIBE_RPC_CMD,
	BT_LE_PER_ADV_SYNC_TRANSFER_UNSUBSCRIBE_RPC_CMD,
	BT_LE_PER_ADV_LIST_ADD_RPC_CMD,
	BT_LE_PER_ADV_LIST_REMOVE_RPC_CMD,
	BT_LE_PER_ADV_LIST_CLEAR_RPC_CMD,
	BT_LE_SCAN_STOP_RPC_CMD,
	BT_LE_SCAN_CB_REGISTER_ON_REMOTE_RPC_CMD,
	BT_LE_WHITELIST_ADD_RPC_CMD,
	BT_LE_WHITELIST_REM_RPC_CMD,
	BT_LE_WHITELIST_CLEAR_RPC_CMD,
	BT_LE_SET_CHAN_MAP_RPC_CMD,
	BT_LE_OOB_GET_LOCAL_RPC_CMD,
	BT_LE_EXT_ADV_OOB_GET_LOCAL_RPC_CMD,
	BT_UNPAIR_RPC_CMD,
	BT_FOREACH_BOND_RPC_CMD,
	/* conn.h API */
	BT_CONN_REMOTE_UPDATE_REF_RPC_CMD,
	BT_CONN_GET_INFO_RPC_CMD,
	BT_CONN_GET_REMOTE_INFO_RPC_CMD,
	BT_CONN_LE_PARAM_UPDATE_RPC_CMD,
	BT_CONN_LE_DATA_LEN_UPDATE_RPC_CMD,
	BT_CONN_LE_PHY_UPDATE_RPC_CMD,
	BT_CONN_DISCONNECT_RPC_CMD,
	BT_CONN_LE_CREATE_RPC_CMD,
	BT_CONN_LE_CREATE_AUTO_RPC_CMD,
	BT_CONN_CREATE_AUTO_STOP_RPC_CMD,
	BT_LE_SET_AUTO_CONN_RPC_CMD,
	BT_CONN_SET_SECURITY_RPC_CMD,
	BT_CONN_GET_SECURITY_RPC_CMD,
	BT_CONN_ENC_KEY_SIZE_RPC_CMD,
	BT_CONN_CB_REGISTER_ON_REMOTE_RPC_CMD,
	BT_SET_BONDABLE_RPC_CMD,
	BT_SET_OOB_DATA_FLAG_RPC_CMD,
	BT_LE_OOB_SET_LEGACY_TK_RPC_CMD,
	BT_LE_OOB_SET_SC_DATA_RPC_CMD,
	BT_LE_OOB_GET_SC_DATA_RPC_CMD,
	BT_PASSKEY_SET_RPC_CMD,
	BT_CONN_AUTH_CB_REGISTER_ON_REMOTE_RPC_CMD,
	BT_CONN_AUTH_PASSKEY_ENTRY_RPC_CMD,
	BT_CONN_AUTH_CANCEL_RPC_CMD,
	BT_CONN_AUTH_PASSKEY_CONFIRM_RPC_CMD,
	BT_CONN_AUTH_PAIRING_CONFIRM_RPC_CMD,
	BT_CONN_FOREACH_RPC_CMD,
	BT_CONN_LOOKUP_ADDR_LE_RPC_CMD,
	BT_CONN_GET_DST_OUT_RPC_CMD,
};

/** @brief Host commands IDs used in bluetooth API serialization.
 *         Those commands are sent from the host to the client.
 */
enum bt_rpc_cmd_from_host_to_cli
{
	/* bluetooth.h API */
	BT_LE_SCAN_CB_T_CALLBACK_RPC_CMD,
	BT_LE_EXT_ADV_CB_SENT_CALLBACK_RPC_CMD,
	BT_LE_EXT_ADV_CB_SCANNED_CALLBACK_RPC_CMD,
	BT_LE_EXT_ADV_CB_CONNECTED_CALLBACK_RPC_CMD,
	BT_LE_SCAN_CB_RECV_RPC_CMD,
	BT_LE_SCAN_CB_TIMEOUT_RPC_CMD,
	BT_FOREACH_BOND_CB_CALLBACK_RPC_CMD,
	PER_ADV_SYNC_CB_SYNCED_RPC_CMD,
	PER_ADV_SYNC_CB_TERM_RPC_CMD,
	PER_ADV_SYNC_CB_RECV_RPC_CMD,
	PER_ADV_SYNC_CB_STATE_CHANGED_RPC_CMD,
	/* conn.h API */
	BT_CONN_CB_CONNECTED_CALL_RPC_CMD,
	BT_CONN_CB_DISCONNECTED_CALL_RPC_CMD,
	BT_CONN_CB_LE_PARAM_REQ_CALL_RPC_CMD,
	BT_CONN_CB_LE_PARAM_UPDATED_CALL_RPC_CMD,
	BT_CONN_CB_LE_PHY_UPDATED_CALL_RPC_CMD,
	BT_CONN_CB_LE_DATA_LEN_UPDATED_CALL_RPC_CMD,
	BT_CONN_CB_IDENTITY_RESOLVED_CALL_RPC_CMD,
	BT_CONN_CB_SECURITY_CHANGED_CALL_RPC_CMD,
	BT_CONN_CB_REMOTE_INFO_AVAILABLE_CALL_RPC_CMD,
	BT_RPC_AUTH_CB_PAIRING_ACCEPT_RPC_CMD,
	BT_RPC_AUTH_CB_PASSKEY_DISPLAY_RPC_CMD,
	BT_RPC_AUTH_CB_PASSKEY_ENTRY_RPC_CMD,
	BT_RPC_AUTH_CB_PASSKEY_CONFIRM_RPC_CMD,
	BT_RPC_AUTH_CB_OOB_DATA_REQUEST_RPC_CMD,
	BT_RPC_AUTH_CB_CANCEL_RPC_CMD,
	BT_RPC_AUTH_CB_PAIRING_CONFIRM_RPC_CMD,
	BT_RPC_AUTH_CB_PINCODE_ENTRY_RPC_CMD,
	BT_RPC_AUTH_CB_PAIRING_COMPLETE_RPC_CMD,
	BT_RPC_AUTH_CB_PAIRING_FAILED_RPC_CMD,
	BT_CONN_FOREACH_CB_CALLBACK_RPC_CMD,
};

/** @brief Host events IDs used in bluetooth API serialization.
 *         Those events are sent from the host to the client.
 */
enum bt_rpc_evt_from_host_to_cli {
	/* bluetooth.h API */
	BT_READY_CB_T_CALLBACK_RPC_EVT,
};

/** @brief Pairing flags IDs. Those flags are used to setup valid callback sets on
 *         the host side. */
enum {
	FLAG_PAIRING_ACCEPT_PRESENT   = BIT(0),
	FLAG_PASSKEY_DISPLAY_PRESENT  = BIT(1),
	FLAG_PASSKEY_ENTRY_PRESENT    = BIT(2),
	FLAG_PASSKEY_CONFIRM_PRESENT  = BIT(3),
	FLAG_OOB_DATA_REQUEST_PRESENT = BIT(4),
	FLAG_CANCEL_PRESENT           = BIT(5),
	FLAG_PAIRING_CONFIRM_PRESENT  = BIT(6),
	FLAG_PAIRING_COMPLETE_PRESENT = BIT(7),
	FLAG_PAIRING_FAILED_PRESENT   = BIT(8),
};

/* Helper callback definitions for connection API and Extended Advertising.
 * Below callbacks have no theirs own typedefs in Bluetooth API, but instead
 * they are defined directly in structures or function parameters. Decoders and
 * encoders use variables of that types, so typedef is convinient in this place.
 */
typedef void (*bt_conn_foreach_cb)(struct bt_conn *conn, void *data);
typedef void (*bt_foreach_bond_cb)(const struct bt_bond_info *info,
				   void *user_data);
typedef void (*bt_le_ext_adv_cb_sent)(struct bt_le_ext_adv *adv,
		     struct bt_le_ext_adv_sent_info *info);
typedef void (*bt_le_ext_adv_cb_connected)(struct bt_le_ext_adv *adv,
			  struct bt_le_ext_adv_connected_info *info);
typedef void (*bt_le_ext_adv_cb_scanned)(struct bt_le_ext_adv *adv,
			struct bt_le_ext_adv_scanned_info *info);

NRF_RPC_GROUP_DECLARE(bt_rpc_grp);

#if defined(CONFIG_BT_RPC_HOST)
/** @brief Read configuration "check list" from the host.
 * 
 * @param[out] data Pointer to received configuration "check list".
 * @param[in]  size Size of data buffer.
 */
void bt_rpc_get_check_list(uint8_t *data, size_t size);
#else
/** @brief Validate configuration "check list".
 * 
 * @param[in] data Pointer to configuration "check list".
 * @param[in] size Size of the configuration "check list".
 * 
 * @retval True if provided "check list" is matches with client's own "check list".
 *         Otherwise, false.
 */
bool bt_rpc_validate_check_list(uint8_t* data, size_t size);

/** @brief Get configuration "check list" size.
 * 
 * @retval Configuration "check list" size.
 */
size_t bt_rpc_calc_check_list_size(void);
#endif

/** @brief Encode Bluetooth connection object.
 * 
 * @param[in, out] encoder Cbor Encoder instance.
 * @param[in] conn Connection object to encode.
 */
void bt_rpc_encode_bt_conn(CborEncoder *encoder, const struct bt_conn *conn);

/** @brief Decode Bluetooth connection object.
 * 
 * @param[in] value Cbor Value to decode.
 * 
 * @retval Connection object.
 */
struct bt_conn *bt_rpc_decode_bt_conn(CborValue *value);

#endif /* BT_RPC_COMMON_H_ */
