#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE 1024
#define THREAD_PRIORITY 5

K_THREAD_STACK_DEFINE(thread_1_stack_area, STACK_SIZE);
static struct k_thread thread_1;

K_THREAD_STACK_DEFINE(thread_2_stack_area, STACK_SIZE);
static struct k_thread thread_2;

K_MUTEX_DEFINE(mutex_counter);

volatile uint32_t counter = 0;

void increment_counter(void *p1, void *p2, void *p3)
{
    for (volatile uint32_t i = 0; i < 1000000; i++)
    {
        k_mutex_lock(&mutex_counter, K_FOREVER);
        counter++;
        k_mutex_unlock(&mutex_counter);
    }
}

int main(void)
{
    k_thread_create(&thread_1, thread_1_stack_area,
                    K_THREAD_STACK_SIZEOF(thread_1_stack_area),
                    increment_counter, NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_FOREVER);
    

    k_thread_create(&thread_2, thread_2_stack_area,
                    K_THREAD_STACK_SIZEOF(thread_2_stack_area),
                    increment_counter, NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_FOREVER);

    k_thread_start(&thread_1);
    k_thread_start(&thread_2);

    k_thread_join(&thread_1, K_FOREVER);
    k_thread_join(&thread_2, K_FOREVER);

    LOG_INF("Counter: %u", counter);

    return 0;
}

