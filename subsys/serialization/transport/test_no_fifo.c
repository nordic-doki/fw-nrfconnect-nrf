
#include <stdint.h>

struct rp_trans_endpoint;

typedef int (*rp_trans_callback_t)(struct rp_trans_endpoint *ep, uint8_t *packet, size_t length);
typedef int (*rp_trans_filter_t)(struct rp_trans_endpoint *ep, uint8_t **packet, int *result);

struct rp_trans_endpoint
{
	int dummy;
};

/** @brief Own endpoint's responsibility for incoming packets.
 *
 * Calling thread tells the transport that it owns responsibility for fetching
 * and handling incoming packets. After call all incoming packets
 * will not go into the endpoint's receive thread (or an IRQ), but they will
 * wait until this thread reads it with @a rp_trans_read_start or give
 * responsibility back with @a rp_trans_give. This function also works as a
 * mutex i.e. if some other thread will try to own the endpoint it will wait.
 * 
 * Owning is recursice, so calling thread may call this function multiple
 * times. Responsibility is actually given back when @a rp_trans_give was
 * called the same number of times.
 */
void rp_trans_own(struct rp_trans_endpoint *ep);
void rp_trans_give(struct rp_trans_endpoint *ep);

int rp_trans_read_start(struct rp_trans_endpoint *ep, uint8_t **packet, rp_trans_filter_t filter_callback);
void rp_trans_read_end(struct rp_trans_endpoint *ep);


typedef void (*enum_services_callback)(const char* name);
typedef void (*rp_ser_resp_decoder)(struct decode_buffer *dec, void* result);
 
// callback called from transport endpoint receive thread
static void rp_trans_rx_callback(struct rpser_endpoint *ep, u8_t* buf, size_t length)
{
	handle_packet(ep, buf, length);
}

// called from endpoint thread or loop waiting for response
void handle_packet(u8_t* data, size_t length)
{
        packet_type type = data[0];
        // We get response - this kind of packet should be handled before
        __ASSERT(type != PACKET_TYPE_RESPONSE, "Response packet without any call");
 
        // This contition may only be true in endpoint thread (maybe add assert?)
        if (type == PACKET_TYPE_ACK) {
                ep->waiting_for_ack = false;
                return;
        }
 
        decoder_callback decoder = get_decoder_from_data(data);
 
        // If we are executing command then the other end is waiting for
        // response, so sending notifications and commands is available again now.
        bool prev_wait_for_ack = ep->waiting_for_ack;
        if (packet_type == PACKET_TYPE_CMD) {
                ep->waiting_for_ack = false;
        }
 
        decoder(type, &data[5], length - 5);
 
        // Resore previous state of waiting for ack
        if (packet_type == PACKET_TYPE_CMD) {
                ep->waiting_for_ack = prev_wait_for_ack;
        }
}
 

typedef uint64_t decoder_old_state_t;

decoder_old_state_t set_decoder(struct rpser_endpoint *ep, rp_ser_resp_decoder decoder, void *result)
{
	// TODO: Different platforms
	uint64_t state = (uint64_t)(uintptr_t)(void*)ep->decoder + (uint64_t)(uintptr_t)ep->decoder_result << 32;
	ep->decoder = decoder;
	ep->decoder_result = result;
	return state;
}

#define PACKET_FILTERED_OUT (-0x7F000000)

int response_filter(struct rp_trans_endpoint *ep, uint8_t **packet, int *result)
{
	uint8_t* data = *packet;
	if (data[0] == PACKET_TYPE_RESPONSE) {
		ep->decoder(data, *result);
		*result = PACKET_FILTERED_OUT;
		return 1;
	}
	return 0;
}

// Call after send of command to wait for response
void wait_for_response(struct rpser_endpoint *ep, rp_ser_resp_decoder decoder, void *result)
{
	uint8_t *packet;
	int packet_length;
	packet_type type;

	do {
	        // Wait for something from rx callback
	        packet_length = rp_trans_read_start(&ep->trans_ep, &packet, response_filter);
		if (packet_length == PACKET_FILTERED_OUT)
		{
			break;
		}
                switch (packet[0])
		{
		case PACKET_TYPE_CMD:
		case PACKET_TYPE_NOTIFY:
			// rp_trans_read_end will be called indirectly from command/event decoder
			handle_packet(ep, packet, packet_length);
			break;
		default:
			// TODO: Invalid State Error Log
			rp_trans_read_end();
			break;
		}
	} while (true);
}

int event_ack_filter(struct rp_trans_endpoint *ep, uint8_t **packet, int *result)
{
	uint8_t* data = *packet;
	if (data[0] == PACKET_TYPE_ACK) {
		*result = PACKET_FILTERED_OUT;
		return 1;
	}
	return 0;
}

// Called before sending command or notify to make sure that last notification was finished and the other end
// can handle this packet imidetally.
void wait_for_last_ack(struct rpser_endpoint *ep)
{
	uint8_t *packet;
	int packet_length;
	packet_type type;

        if (!ep->waiting_for_ack) {
		return;
	}

	do {
	        // Wait for something from rx callback
	        packet_length = rp_trans_read_start(&ep->trans_ep, &packet, event_ack_filter);
		if (packet_length == PACKET_FILTERED_OUT)
		{
			ep->waiting_for_ack = false;
			return;
		}

                switch (packet[0])
		{
		case PACKET_TYPE_CMD:
		case PACKET_TYPE_NOTIFY:
			// rp_trans_read_end will be called indirectly from command/event decoder
			handle_packet(ep, packet, packet_length);
			break;
		default:
			// TODO: Invalid State Error Log
			rp_trans_read_end();
			break;
		}
	} while (true);
}
 
// Call remote function (command): send cmd and data, wait and return response
void call_remote(struct rpser_endpoint *ep, struct decode_buffer *in, rp_ser_resp_decoder decoder, void *result)
{
	decoder_old_state_t old_state;
        // Endpoint is not accessible by other thread from this point
	rp_trans_own(&ep->trans_ep);
        // Make sure that someone can handle packet immidietallty
        wait_for_last_ack(ep);
	// Set decoder for current command and save on stack decoder for previously waiting response
	old_state = set_decoder(ep, decoder, result);
        // Send buffer to transport layer
        rp_trans_send(&ep->trans_ep, in->data, in->length);
        // Wait for response. During waiting nested commands and notifications are possible
        wait_for_response(ep, decoder, result);
        // restore decoder for previously waiting response
	restore_decoder(ep, old_state);
	// 
	rp_trans_give(&ep->trans_ep);
}
 
// Execute notification: send notification, do not wait - only mark that we are expecting ack later
void notify_remote(struct rpser_endpoint *ep, struct decode_buffer *in)
{
        // Endpoint is not accessible by other thread from this point
	rp_trans_own(&ep->trans_ep);
        // Make sure that someone can handle packet immidietallty
        wait_for_last_ack(ep);
        // we are expecting ack later
        ep->waiting_for_ack = true;
        // Send buffer to transport layer
        rp_trans_send(&ep->trans_ep, in->data, in->length);
        // We can unlock now, nothing more to do
	rp_trans_give(&ep->trans_ep);
}
 
// Inform that decoding is done and nothing more must be done
void decode_response_done(struct rpser_endpoint *ep)
{
        k_sem_give(ep->decode_done_sem);
        k_mutex_unlock(ep->mutex);
}

static void enum_services_decoder(struct decode_buffer *dec, void* result)
{
	*((int *)result) = decode_int(dec);
}
 
// example function
int enum_services(enum_services_callback callback, int max_count)
{
        int result;
        struct encode_buffer buf;
        struct decode_buffer dec;
        // encode params
        encode_init_cmd(&buf, ENUM_SERVICES_ID);
        encode_ptr(&buf, callback);
        encode_int(&buf, max_count);
        // call and wait
        call_remote(&my_ep, &buf, enum_services_decoder, &result);
        return result;
}
 
// example function
int enum_services2(enum_services_callback callback, int max_count)
{
        int result;
        struct encode_buffer buf;
        struct decode_buffer dec;
        // encode params
        encode_init_cmd(&buf, ENUM_SERVICES_ID);
        encode_ptr(&buf, callback);
        encode_int(&buf, max_count);
        // call and wait
        call_remote(&my_ep, &buf, enum_services_decoder, &result);
        return result;
}
 
// example notification
void notify_update(int count)
{
        struct encode_buffer buf;
        // encode params
        encode_init_notification(&buf, NOTIFY_UPDATE_ID);
        encode_int(&buf, count);
        // send notification
        notify_remote(&my_ep, &buf);
}
 
// called by decoder to inform rx callback that decoding is done and it can continue (discard buffers)
void decode_params_done(struct rpser_endpoint *ep)
{
        k_sem_give(ep->decode_done_sem);
}
 
// example decoder (universal: for both commands and notifications)
void call_callback_b_s(struct rpser_endpoint *ep, struct decode_buffer *dec)
{
        bool result;
        char str[32];
        enum_services_callback callback;
        struct encode_buffer buf;
        // decode params
        decode_ptr(dec, &callback);
        decode_str(dec, str, 32);
        // inform rx callback that decoding is done and it can continue (discard buffers)
        decode_params_done(ep);
        // execute actual code
        result = callback(name);
        if (type == PACKET_TYPE_CMD) {
                // cndode response
                encode_init_response(&buf);
                encode_bool(&buf, result);
                // and send response
                send_response(ep, buf);
        } else {
                // send that notification is done
                send_notify_ack(ep);
        }
}