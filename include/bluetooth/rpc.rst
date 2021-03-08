.. _ble_rpc:

Bluetooth Low Energy Remote Procedure call
##########################################

.. contents::
   :local:
   :depth: 2

The nRF Connect SDK has support for the Bluetooth Low Energy stack serialization.
The full Bluetooth Low Energy stack can be ran on the another device or CPU in particular on the nRF5340 DK Network Core using :ref:`nrfxlib:nrf_rpc`.

.. note::
   Currently a serialization of the :ref:`zephyr:bt_gap` and :ref:`zephyr:bluetooth_connection_mgmt` is supported.
   The support for the :ref:`zephyr:bt_gatt` will be added in a future.

Network Core
************

The :ref:`ble_rpc_host` sample is designed specifically to enable the Bluetooth LE stack functionality on a remote MCU (for example, the nRF5340 network core) using the :ref:`nrfxlib:nrf_rpc`.
This sample can be programmed to the network core to run standard Bluetooth Low Energy samples on nRF5340.
You can choose whether to use the SoftDevice Controller or the Zephyr Bluetooth LE Controller for this sample.

Application Core
****************

Bluetooth sample which are running on Application sample must be built with additional configuration.
When building samples for the Application core, enable the :option:`CONFIG_BT_RPC` to run Bluetooth Low Energy stack on the Network Core.
This option also causes that the :ref:`ble_rpc_host` is built automatically as a child image.
For more details see: :ref:`ug_nrf5340_building`.

.. tabs::

   .. group-tab:: west

      Open a command prompt in the build folder of the application sample and enter the following command to build application for the Application Core and :ref:`ble_rpc_host` as child image::

        west build -b nrf5340dk_nrf5340_cpuapp -- -DBT_RPC=y

Requirements
************

Some configuration options related to the Bluetooth Low Energy must be equal on the host (Network core) and client (Application core).
The following options must have been set in the same way for the :ref:`ble_rpc_host` and the Application Core sample:

   * :option:`CONFIG_BT_CENTRAL`
   * :option:`CONFIG_BT_PERIPHERAL`
   * :option:`CONFIG_BT_WHITELIST`
   * :option:`CONFIG_BT_USER_PHY_UPDATE`
   * :option:`CONFIG_BT_USER_DATA_LEN_UPDATE`
   * :option:`CONFIG_BT_PRIVACY`
   * :option:`CONFIG_BT_SCAN_WITH_IDENTITY`
   * :option:`CONFIG_BT_REMOTE_VERSION`
   * :option:`CONFIG_BT_SMP`
   * :option:`CONFIG_BT_CONN`
   * :option:`CONFIG_BT_REMOTE_INFO`
   * :option:`CONFIG_BT_FIXED_PASSKEY`
   * :option:`CONFIG_BT_SMP_APP_PAIRING_ACCEPT`
   * :option:`CONFIG_BT_EXT_ADV`
   * :option:`CONFIG_BT_OBSERVER`
   * :option:`CONFIG_BT_ECC`
   * :option:`CONFIG_BT_DEVICE_NAME_DYNAMIC`
   * :option:`CONFIG_BT_SMP_SC_PAIR_ONLY`
   * :option:`CONFIG_BT_PER_ADV`
   * :option:`CONFIG_BT_PER_ADV_SYNC`
   * :option:`CONFIG_BT_MAX_CONN`
   * :option:`CONFIG_BT_ID_MAX`
   * :option:`CONFIG_BT_EXT_ADV_MAX_ADV_SET`
   * :option:`CONFIG_BT_DEVICE_NAME_MAX`
   * :option:`CONFIG_BT_DEVICE_NAME_MAX`
   * :option:`CONFIG_BT_PER_ADV_SYNC_MAX`
   * :option:`CONFIG_BT_DEVICE_NAME`
   * :option:`CONFIG_CBKPROXY_OUT_SLOT` equal to :option:`CONFIG_CBKPROXY_IN_SLOTS`
