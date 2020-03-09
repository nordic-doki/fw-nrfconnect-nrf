/** @file
 *  @brief Bluetooth subsystem core APIs.
 */

/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_BLUETOOTH_BLUETOOTH_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_BLUETOOTH_H_

#ifndef SERIALIZABLE
#define SERIALIZABLE(...)
#endif

/**
 * @brief Bluetooth APIs
 * @defgroup bluetooth Bluetooth APIs
 * @{
 */

#include <stdbool.h>
#include <string.h>
#include <sys/printk.h>
#include <sys/util.h>
#include <net/buf.h>
#include <bluetooth/gap.h>
#include <bluetooth/addr.h>
#include <bluetooth/crypto.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generic Access Profile
 * @defgroup bt_gap Generic Access Profile
 * @ingroup bluetooth
 * @{
 */

/** @def BT_ID_DEFAULT
 *
 *  Convenience macro for specifying the default identity. This helps
 *  make the code more readable, especially when only one identity is
 *  supported.
 */
#define BT_ID_DEFAULT 0

/**
 * @typedef bt_ready_cb_t
 * @brief Callback for notifying that Bluetooth has been enabled.
 *
 *  @param err zero on success or (negative) error code otherwise.
 */
typedef void (*bt_ready_cb_t)(int err);
	SERIALIZABLE(NOTIFY);


int bt_enable(bt_ready_cb_t cb);
	SERIALIZABLE();


int bt_set_name(const char *name);
	SERIALIZABLE();

const char *bt_get_name(void);
	SERIALIZABLE(TODO("decoder: keep it in local variable"));
	SERIALIZABLE(IGNORE($));


SERIALIZABLE(STRUCT(bt_addr_le_t));


int bt_set_id_addr(const bt_addr_le_t *addr);
	SERIALIZABLE();


void bt_id_get(bt_addr_le_t *addrs, size_t *count);
	SERIALIZABLE(OUT(addrs));
	SERIALIZABLE(INOUT(count));
	SERIALIZABLE(ARRAY_LENGTH(addrs, count));


int bt_id_create(bt_addr_le_t *addr, u8_t *irk);
	SERIALIZABLE(ARRAY_LENGTH_CONST(irk, 16));


int bt_id_reset(u8_t id, bt_addr_le_t *addr, u8_t *irk);
	SERIALIZABLE(ARRAY_LENGTH_CONST(irk, 16));


int bt_id_delete(u8_t id);
	SERIALIZABLE(STR_MAX_LEN(name, 64));

struct bt_data {
	u8_t type;
	u8_t data_len;
	const u8_t *data;
};
	SERIALIZABLE(ARRAY_LENGTH(data, data_len);

struct bt_le_adv_param {
	u8_t  id;
	u8_t  options;
	u16_t interval_min;
	u16_t interval_max;
};
	SERIALIZABLE();

int bt_le_adv_start(const struct bt_le_adv_param *param,
		    const struct bt_data *ad, size_t ad_len,
		    const struct bt_data *sd, size_t sd_len);
	SERIALIZABLE(ARRAY_LENGTH(ad, ad_len));
	SERIALIZABLE(ARRAY_LENGTH(sd, sd_len));

int bt_le_adv_update_data(const struct bt_data *ad, size_t ad_len,
			  const struct bt_data *sd, size_t sd_len);
	SERIALIZABLE(ARRAY_LENGTH(ad, ad_len));
	SERIALIZABLE(ARRAY_LENGTH(sd, sd_len));

int bt_le_adv_stop(void);
	SERIALIZABLE();

SERIALIZABLE(STRUCT(net_buf_simple));
	SERIALIZABLE(TODO("Write custom code"));

typedef void bt_le_scan_cb_t(const bt_addr_le_t *addr, s8_t rssi,
			     u8_t adv_type, struct net_buf_simple *buf);
	SERIALIZABLE()

struct bt_le_scan_param {
	u8_t  type;
	u8_t  filter_dup;
	u16_t interval;
	u16_t window;
};
	SERIALIZABLE();

struct bt_le_adv_info {
	const bt_addr_le_t *addr;
	s8_t rssi;
	u8_t adv_type;
};
	SERIALIZABLE();

struct bt_le_scan_cb {
	void (*recv)(const struct bt_le_adv_info *info,
		     struct net_buf_simple *buf);
#ifndef SERIALIZATION_ACTIVE
	sys_snode_t node;
#endif
};

SERIALIZABLE(CALLBACK(bt_le_scan_cb::recv));
	SERIALIZABLE(NOTIFY);

int bt_le_scan_start(const struct bt_le_scan_param *param, bt_le_scan_cb_t cb);
	SERIALIZABLE();

int bt_le_scan_stop(void);
	SERIALIZABLE();

void bt_le_scan_cb_register(struct bt_le_scan_cb *cb);
	SERIALIZABLE();

int bt_le_whitelist_add(const bt_addr_le_t *addr);
	SERIALIZABLE();

int bt_le_whitelist_rem(const bt_addr_le_t *addr);
	SERIALIZABLE();

int bt_le_whitelist_clear(void);
	SERIALIZABLE();

int bt_le_set_chan_map(u8_t chan_map[5]);
	SERIALIZABLE(ARRAY_LENGTH_CONST(chan_map, 5));

/* Should not be serializable as it does not work on bluetooth.
 * net_buf_simple should be converted to something else for non-zephyr
 * compability.
 */
void bt_data_parse(struct net_buf_simple *ad,
		   bool (*func)(struct bt_data *data, void *user_data),
		   void *user_data);

struct bt_le_oob_sc_data {
	u8_t r[16];
	u8_t c[16];
};
	SERIALIZABLE(ARRAY_SIZE_CONST(r, 16));
	SERIALIZABLE(ARRAY_SIZE_CONST(c, 16));

struct bt_le_oob {
	bt_addr_le_t addr;
	struct bt_le_oob_sc_data le_sc_data;
};
	SERIALIZABLE();

int bt_le_oob_get_local(u8_t id, struct bt_le_oob *oob);
	SERIALIZABLE();

struct bt_br_discovery_result {
	u8_t _priv[4];
	bt_addr_t addr;
	s8_t rssi;
	u8_t cod[3];
	u8_t eir[240];
};
	SERIALIZABLE(ARRAY_LENGTH_CONST(_priv, 4));
	SERIALIZABLE(ARRAY_LENGTH_CONST(cod, 3));
	SERIALIZABLE(ARRAY_LENGTH_CONST(eir, 240));
	SERIALIZABLE(TODO("eir should be reduced if not needed, e.g."));
#if _TODO_
	SERIALIZABLE(EXTRA(size_t eir_size));
	SERIALIZABLE(ARRAY_SIZE(eir, eir_size));
	// and code for encode:
	// eir_size = calculate_eir_size(d->eir);
	// for decode:
	// memset(&d->eir[eir_size], 0, 240 - eir_size);
#endif

typedef void bt_br_discovery_cb_t(struct bt_br_discovery_result *results,
				  size_t count);
	SERIALIZABLE(TODO("Encoder: memory allocated for `results` can be deleted now (probably - need to check)"));
	SERIALIZABLE(TODO("Decoder: decode `results` manually to buffer provided on bt_br_discovery_start"));

struct bt_br_discovery_param {
	u8_t length;
	bool limited;
};
	SERIALIZABLE();


int bt_br_discovery_start(const struct bt_br_discovery_param *param,
			  struct bt_br_discovery_result *results, size_t count,
			  bt_br_discovery_cb_t cb);
	SERIALIZABLE(IGNORE(results))
	SERIALIZABLE(TODO("Encoder: Keep results pointer for later use in bt_br_discovery_cb_t"));
	SERIALIZABLE(TODO("Decoder: Allocate new memory for results"));

int bt_br_discovery_stop(void);
	SERIALIZABLE(TODO("Decoder: Free memory for results if not deleted yet"));

struct bt_br_oob {
	bt_addr_t addr;
};
	SERIALIZABLE();

int bt_br_oob_get_local(struct bt_br_oob *oob);
	SERIALIZABLE(OUT(oob));

/* not serializable */
int bt_addr_to_str(const bt_addr_t *addr, char *str, size_t len);

/* not serializable */
static inline int bt_addr_le_to_str(const bt_addr_le_t *addr, char *str,
				    size_t len);

/* not serializable */
int bt_addr_from_str(const char *str, bt_addr_t *addr);

/* not serializable */
int bt_addr_le_from_str(const char *str, const char *type, bt_addr_le_t *addr);

int bt_br_set_discoverable(bool enable);
	SERIALIZABLE();

int bt_br_set_connectable(bool enable);
	SERIALIZABLE();

int bt_unpair(u8_t id, const bt_addr_le_t *addr);
	SERIALIZABLE();

struct bt_bond_info {
	bt_addr_le_t addr;
};
	SERIALIZABLE();

void bt_foreach_bond(u8_t id, void (*func)(const struct bt_bond_info *info,
					   void *user_data),
		     void *user_data);
	SERIALIZABLE(POINTER_VALUE(user_data));
	SERIALIZABLE(CALLBACK(bt_foreach_bond::func));
		SERIALIZABLE(NOTIFY);


/**
 * @}
 */

#ifdef __cplusplus
}
#endif
/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_BLUETOOTH_H_ */
