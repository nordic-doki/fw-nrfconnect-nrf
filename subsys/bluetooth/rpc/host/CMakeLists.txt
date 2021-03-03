#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_sources(bt_rpc_gap_host.c)
zephyr_sources_ifdef(CONFIG_BT_CONN bt_rpc_conn_host.c)
