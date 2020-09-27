#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

zephyr_sources(rtt_lite_trace.c)

zephyr_library_include_directories(
    ${ZEPHYR_BASE}/kernel/include
    ${ZEPHYR_BASE}/arch/${ARCH}/include
    .
    )