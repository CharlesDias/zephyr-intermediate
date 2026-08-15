#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE 1024

#define PRIO_COOP (-1)
#define PRIO_LOW 7
#define PRIO_MED 5
#define PRIO_HIGH 3

void thread_coop_fn(void *p1, void *p2, void *p3)
{
    for (int i = 0; i < 5; i++) {
        k_busy_wait(200000);
        LOG_INF("[T_COOP] iterations %d/5", i + 1);
    }

    k_yield();
}

void thread_fn(void *p1, void *p2, void *p3)
{
    const char *name = p1;
    int32_t ms_sleep = (int32_t)POINTER_TO_INT(p2);
    uint32_t loop = 0;

    while (1) {
        k_msleep(ms_sleep);
        LOG_INF("%s running loop=%u", name, ++loop);
    }
}

K_THREAD_DEFINE(thread_coop, STACK_SIZE, thread_coop_fn,
                NULL, NULL, NULL, PRIO_COOP, 0, 0);
K_THREAD_DEFINE(thread_low, STACK_SIZE, thread_fn,
                "T_LOW", 300, NULL, PRIO_LOW, 0, 0);
K_THREAD_DEFINE(thread_med, STACK_SIZE, thread_fn,
                "T_MED", 200, NULL, PRIO_MED, 0, 0);
K_THREAD_DEFINE(thread_high, STACK_SIZE, thread_fn,
                "T_HIGH", 100, NULL, PRIO_HIGH, 0, 0);

int main(void)
{
    return 0;
}

