.. _rtt_light_trace:

RTT light trace
###############

TODO: doc

Example user defined events and markers:

Definitions:
	#define SEND_PACKET      RTT_LITE_TRACE_USER_EVENT(0)
	#define SEND_PACKET_EXIT RTT_LITE_TRACE_USER_EVENT(1)
	#define PACKET_DROPPED   RTT_LITE_TRACE_USER_EVENT(2)

	#define MAIN_LOOP_MARKER 1
	#define TX_PACKET_MARKER 2

Usage:
	rtt_lite_trace_call_1(SEND_PACKET, packet_size);
	// ...
	rtt_lite_trace_call_1(SEND_PACKET_EXIT, send_result);

	rtt_lite_trace_mark_start(MAIN_LOOP_MARKER)
	// ...
	rtt_lite_trace_mark_stop(MAIN_LOOP_MARKER)

Description:
	NamedType Err 0=ERR_NONE 1=ERR_BUFFER_OVERFLOW 2=ERR_UNKNOWN
	Event     0   send_packet     size=%u  |  1  %Err
	Event     2   packet_dropped  size=%u
	Marker    1   MAIN_LOOP
	Marker    2   TX_PACKET

API documentation
*****************

| Header file: :file:`include/debug/rtt_light_trace.h`
| Source files: :file:`subsys/debug/rtt_light_trace/`

.. doxygengroup:: rtt_light_trace
   :project: nrf
   :members:
