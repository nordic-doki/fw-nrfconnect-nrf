.. _ble_rpc_host:

Bluetooth: Host for nRF RPC BLE
###############################
The nRF RPC Host sample demonstrates how to run the Bluetooth Low Energy stack on the another device or CPU using :ref:`nrfxlib:nrf_rpc` that exchange the function calls between cores using `Remote Procedure Calls (RPC)`_.
The sample makes use of the Bluetooth Low Energy on the Application core of an nRF5340 DK when full stack is running on the Network Core.

Overview
********

.. note::
   Currently a serialization of the :ref:`zephyr:bt_gap` and :ref:`zephyr:bluetooth_connection_mgmt` is supported.

The host (Network Core) is running full Bluetooth Low Energy stack.
The application receives serialized function calls which are decoded and executed and next it sends a response data to the client (Application core). 

When the sample starts, it displays the welcome prompt.

Requirements
************

The sample supports the following development kits:

.. table-from-rows:: /includes/sample_board_rows.txt
   :header: heading
   :rows: nrf5340dk_nrf5340_cpunet

Building and Running
********************
.. |sample path| replace:: :file:`samples/bluetooth/rpc_host`

.. include:: /includes/build_and_run.txt

.. _rpc_host_testing:

Testing
=======

This sample must be build with the same Bluetooth configuration as the Application core sample.
For more details see: :ref:`ble_rpc`.
On the Application Core you can build the :ref:`zephyr: bluetooth-beacon-sample`.
It works out of the box with this sample and does not require configuration change.
In the Beacon sample directory you can invoke:
   .. code-block:: console
      west build -b nrf5340dk_nrf5340_cpuapp -- -DBT_RPC=y


After programming the sample to your development kit, test it by performing the following steps:

1. Connect the dual core development kit to the computer using a USB cable.
   The development kit is assigned a COM port (Windows) or ttyACM device (Linux), which is visible in the Device Manager.
#. |connect_terminal|
#. Reset the development kit.
#. Observe that the terminal connected to the Network core display: "Starting nRF PRC bluetooth host"
#. On the terminal connected to the Application core you can observe your Bluetooth application is running.

Dependencies
************

This sample uses the following libraries:

From |NCS|

* :ref:`ble_rpc`

From nrfxlib

* :ref:`nrfxlib:nrf_rpc`
