.. _nrf-nfc-system-off-sample:

NFC System Off
#####################

Sample shows how to use the NFC tag to wake up from System OFF mode by NFC field detect.
It uses the :ref:`nfc`.

Overview
********

When the sample starts, it initializes the NFC tag.
Then it sense the external NFC field.
If field is not detected for 3 seconds it goes into system off mode.
Wake up from system off mode will be done when NFC field is detected.
Then system is started and NFC tag reinitialized again.
Tag can be read after that.

Requirements
************

* One of the following development boards:

  * |nRF52840DK|
  * |nRF52DK|

* Smartphone or tablet

User interface
**************

LED 1:
   Indicates if an NFC field is present.

LED 2:
   Indicates system on mode.

Building and running
********************

.. |sample path| replace:: :file:`samples/nfc/system_off`

.. include:: /includes/build_and_run.txt

Testing
=======

After programming the sample to your board, test it by performing the following steps:

1. Observe that LED 2 turns off after 3 seconds.
   This indicates that system is in system off mode.
#. Touch the NFC antenna with the smartphone or tablet.
   Observe that LED 2 is lit and LED 1 shortly after that.
#. Observe that the smartphone/tablet displays the encoded text.
#. Move the smartphone/tablet away from the NFC antenna and observe that LED 1 turns off.
#. Observe that LED 2 turns off after 3 seconds.
   This indicates that system is in system off mode again.

Dependencies
************

This sample uses the following |NCS| libraries:

* :ref:`nfc_ndef_msg`
* :ref:`nfc_text`

In addition, it uses the Type 2 Tag library from nrfxlib:

* :ref:`nrfxlib:nfc_api_type2`

The sample uses the following Zephyr libraries:

* ``include/zephyr.h``
* ``include/device.h``
* ``include/power/power.h``
* :ref:`GPIO Interface <zephyr:api_peripherals>`
