/*
 * Lecture 5 - Homework
 *
 * ================================================================
 * Goal: Study reliability under pressure
 *
 * Task:
 *  - Build a producer/consumer pipeline (from Lecture 4 homework)
 *  - Add a task watchdog
 *  - Simulate a stuck consumer (long k_sleep in consumer thread)
 *  - Observe: queue fills, watchdog fires, callback triggered
 *  - Add a health-check thread that monitors queue fill level and logs a warning at 75% capacity
 *
 * Push tag l5-task1
 * ================================================================
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/task_wdt/task_wdt.h>
#include <zephyr/debug/thread_analyzer.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_DBG);

/* Enable/Disable the consumer stuck simulation */
#define ENABLE_CONSUMER_STUCK   1

/* Enable/Disable the consumer slow simulation */
#define ENABLE_CONSUMER_SLOW    1


#define PRODUCER_DELAY_MS       500

#if ENABLE_CONSUMER_SLOW
    #define CONSUMER_DELAY_MS    (PRODUCER_DELAY_MS * 2)
#else
    #define CONSUMER_DELAY_MS    (PRODUCER_DELAY_MS)
#endif

#define CONSUMER_TASK_WDT_TIMEOUT_MS  (CONSUMER_DELAY_MS + 200)

#define HEALTH_CHECK_PRIORITY   4
#define PRODUCER_PRIORITY       5
#define CONSUMER_PRIORITY       6

#define HEALTH_CHECK_STACK_SIZE 512
#define PRODUCER_STACK_SIZE     512
#define CONSUMER_STACK_SIZE     1024

#define SENSOR_QUEUE_DEPTH      8

struct sensor_data {
    uint32_t sequence;
    int32_t temperature_mc;
    uint32_t timestamp_ms;
};

K_MSGQ_DEFINE(sensor_queue, sizeof(struct sensor_data), SENSOR_QUEUE_DEPTH, 4);

static void task_wdt_callback(int channel_id, void *user_data)
{
    LOG_ERR("[WDT-CALLBACK] Task watchdog channel %d callback, thread: %s",
            channel_id, k_thread_name_get((k_tid_t)user_data));
    LOG_ERR("[WDT-CALLBACK] Resetting device...\n\n\n");
    LOG_PANIC();    /* print pending logs before the reset */

    sys_reboot(SYS_REBOOT_COLD);
}

static void health_check_fn(void *p1, void *p2, void *p3)
{
    while(1)
    {
        uint32_t used_percentage = k_msgq_num_used_get(&sensor_queue) * 100 / SENSOR_QUEUE_DEPTH;
    
        if(used_percentage >= 75)
        {
            LOG_WRN("[HEALTH-CHECK] Queue used is about %u%% (%u/%u)",
                    used_percentage, k_msgq_num_used_get(&sensor_queue), SENSOR_QUEUE_DEPTH);
        }
    
        k_msleep(500);
    }
}

static void producer_fn(void *p1, void *p2, void *p3)
{
    uint32_t seq = 0;

    LOG_DBG("[PRODUCER] Starting thread");

    while(1)
    {
        struct sensor_data data = {
            .sequence = seq,
            .temperature_mc = 20000 + (200 * seq),
            .timestamp_ms = k_uptime_get_32()
        };

        seq++;

        LOG_DBG("[PRODUCER] sequence = %2u temp = %d timestamp = %u",
                 data.sequence, data.temperature_mc, data.timestamp_ms);

        int ret = k_msgq_put(&sensor_queue, &data, K_MSEC(100));

        if(ret != 0)
        {
            LOG_WRN("[PRODUCER] Queue is full, dropped sequence = %u", data.sequence);
        }

        k_msleep(500);
    }
}

static void consumer_fn(void *p1, void *p2, void *p3)
{
    int task_wdt_id;

    LOG_DBG("[CONSUMER] Starting thread");

    /*
    * Add a new task watchdog channel with custom callback function and
    * the current thread ID as user data.
    */
    task_wdt_id = task_wdt_add(CONSUMER_TASK_WDT_TIMEOUT_MS, task_wdt_callback, (void *)k_current_get());

    while(1)
    {
        struct sensor_data data;

        int ret = k_msgq_get(&sensor_queue, &data, K_FOREVER);

        if(ret == 0)
        {
            LOG_DBG("[CONSUMER] sequence = %u temp = %d timestamp = %u delay = %u",
                    data.sequence, data.temperature_mc, data.timestamp_ms, k_uptime_get_32 () - data.timestamp_ms);

            #if ENABLE_CONSUMER_STUCK
            if(data.sequence >= 15)
            {
                LOG_WRN("[CONSUMER] Simulating a stuck consumer...");
                while(1)
                {
                    k_msleep(1000);
                }
            }
            #endif
        }
        else
        {
            LOG_WRN("[CONSUMER] Queue get returned an error %d", ret);
        }

        task_wdt_feed(task_wdt_id);
        k_msleep(CONSUMER_DELAY_MS);
    }
}

K_THREAD_DEFINE(health_check_thread, HEALTH_CHECK_STACK_SIZE, health_check_fn, NULL, NULL, NULL, HEALTH_CHECK_PRIORITY, 0, 0);
K_THREAD_DEFINE(producer_thread, PRODUCER_STACK_SIZE, producer_fn, NULL, NULL, NULL, PRODUCER_PRIORITY, 0, 0);
K_THREAD_DEFINE(consumer_thread, CONSUMER_STACK_SIZE, consumer_fn, NULL, NULL, NULL, CONSUMER_PRIORITY, 0, 0);

int main(void)
{
    int ret;
    const struct device *const hw_wdt_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(watchdog0));

    LOG_INF("[MAIN] === L5 Homework: Producer/Consumer ===");
    LOG_DBG("[MAIN] Thread Producer has stack size equal to %d", PRODUCER_STACK_SIZE);
    LOG_DBG("[MAIN] Thread consumer has stack size equal to %d", CONSUMER_STACK_SIZE);
    LOG_DBG("[MAIN] Thread consumer has stack size equal to %d", CONSUMER_STACK_SIZE);

    if (!device_is_ready(hw_wdt_dev)) {
        LOG_ERR("Hardware watchdog not ready!");
        return -1;
    }

    ret = task_wdt_init(hw_wdt_dev);

    if(ret == 0) {
        LOG_DBG("[MAIN] Watchdog initialized!");
    }
    else {
        LOG_ERR("[MAIN] Error when tried to initialized the Watchdog. err = %d", ret);
    }

    while(1)
    {
        thread_analyzer_print(0);
        k_msleep(5000);
    }

    return 0;
}

