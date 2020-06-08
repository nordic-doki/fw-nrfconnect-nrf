
#define NRF_RPC_LOG_MODULE NRF_RPC_OS
#include <nrf_rpc_log.h>

#include "nrf_rpc_os.h"

/* Maximum number of remote thread that this implementation allows. */
#define MAX_REMOTE_THREADS 255


/* Initial value contains ones (context free) on the
 * CONFIG_NRF_RPC_TRANSACTION_POLL_SIZE most significant bits.
 */
#define CONTEXT_MASK_INIT_VALUE \
	(~(((atomic_val_t)1 << (8 * sizeof(atomic_val_t) - CONFIG_NRF_RPC_TRANSACTION_POLL_SIZE)) - 1))

struct pool_start_msg {
	nrf_rpc_os_work_t work;
	void *data;
	size_t len;
};

static struct pool_start_msg pool_start_msg_buf[2];
struct k_msgq pool_start_msg;

static struct k_sem context_reserved;
static atomic_t context_mask;

struct k_sem _nrf_rpc_os_remote_counter;
static uint32_t remote_thread_total;

static K_THREAD_STACK_ARRAY_DEFINE(pool_stacks,
	CONFIG_NRF_RPC_LOCAL_THREAD_POOL_SIZE,
	CONFIG_NRF_RPC_LOCAL_THREAD_STACK_SIZE);
struct k_thread pool_threads[CONFIG_NRF_RPC_LOCAL_THREAD_POOL_SIZE];

BUILD_ASSERT(CONFIG_NRF_RPC_TRANSACTION_POLL_SIZE > 0,
	     "CONFIG_NRF_RPC_TRANSACTION_POLL_SIZE must be greaten than zero");
BUILD_ASSERT(CONFIG_NRF_RPC_TRANSACTION_POLL_SIZE <= 8 * sizeof(atomic_val_t),
	     "CONFIG_NRF_RPC_TRANSACTION_POLL_SIZE too big");
BUILD_ASSERT(sizeof(uint32_t) == sizeof(atomic_val_t),
	     "Only atomic_val_t is implemented that is the same as uint32_t");



static void thread_pool_entry(void *p1, void *p2, void *p3)
{
	struct pool_start_msg msg;

	do {
		k_msgq_get(&pool_start_msg, &msg, K_FOREVER);
		msg.work(msg.data, msg.len);
	} while(1);
}

int nrf_rpc_os_init()
{
	int err;
	int i;
	
	err = k_sem_init(&context_reserved, CONFIG_NRF_RPC_TRANSACTION_POLL_SIZE, CONFIG_NRF_RPC_TRANSACTION_POLL_SIZE);
	if (err < 0) {
		return err;
	}

	err = k_sem_init(&_nrf_rpc_os_remote_counter, 0, MAX_REMOTE_THREADS);
	if (err < 0) {
		return err;
	}
	remote_thread_total = 0;

	atomic_set(&context_mask, CONTEXT_MASK_INIT_VALUE);

	k_msgq_init(&pool_start_msg, (char*)pool_start_msg_buf, sizeof(struct pool_start_msg), ARRAY_SIZE(pool_start_msg_buf));

	for (i = 0; i < CONFIG_NRF_RPC_LOCAL_THREAD_POOL_SIZE; i++) {
		k_thread_create(&pool_threads[i], pool_stacks[i],
			K_THREAD_STACK_SIZEOF(pool_stacks[i]),
			thread_pool_entry,
			NULL, NULL, NULL,
			CONFIG_NRF_RPC_LOCAL_THREAD_PRIORITY, 0, K_NO_WAIT);
	}

	return 0;
}

int nrf_rpc_os_thread_pool_send(nrf_rpc_os_work_t work, void *data, size_t len)
{
	struct pool_start_msg msg;
	msg.work = work;
	msg.data = data;
	msg.len = len;
	k_msgq_put(&pool_start_msg, &msg, K_FOREVER);
	return 0;
}

int nrf_rpc_os_msg_set(struct nrf_rpc_os_msg *msg, void *data, size_t len)
{
	k_sched_lock();
	msg->data = data;
	msg->len = len;
	k_sem_give(&msg->sem);
	k_sched_unlock();
	return 0;
}

int nrf_rpc_os_msg_get(struct nrf_rpc_os_msg *msg, void **data, size_t *len)
{
	k_sem_take(&msg->sem, K_FOREVER);
	k_sched_lock();
	*data = msg->data;
	*len = msg->len;
	k_sched_unlock();
	return 0;
}


int nrf_rpc_os_ctx_pool_reserve()
{
	int number;
	atomic_val_t mask_shadow;
	atomic_val_t this_mask;

	k_sem_take(&context_reserved, K_FOREVER);

	mask_shadow = atomic_get(&context_mask);
	do {
		number = __CLZ(mask_shadow);
		this_mask = (0x80000000u >> number);
		mask_shadow = atomic_and(&context_mask, ~this_mask);
	} while ((mask_shadow & this_mask) == 0);

	NRF_RPC_ERR("----- CTX RESERVED %d", number);

	return number;
}

void nrf_rpc_os_ctx_pool_release(int number)
{
	__ASSERT_NO_MSG(number >= 0);
	__ASSERT_NO_MSG(number < CONFIG_NRF_RPC_TRANSACTION_POLL_SIZE);

	NRF_RPC_ERR("----- CTX RELASE %d", number);

	atomic_or(&context_mask, 0x80000000u >> number);
	k_sem_give(&context_reserved);
}

int nrf_rpc_os_remote_count(int count)
{
	__ASSERT_NO_MSG(count > 0);
	__ASSERT_NO_MSG(count <= MAX_REMOTE_THREADS);

	NRF_RPC_DBG("Remote thread count changed from %d to %d",
		    remote_thread_total, count);

	while ((int)remote_thread_total < count) {
		k_sem_give(&_nrf_rpc_os_remote_counter);
		remote_thread_total++;
	}
	while ((int)remote_thread_total > count) {
		k_sem_take(&_nrf_rpc_os_remote_counter, K_FOREVER);
		remote_thread_total--;
	}

	return 0;
}
