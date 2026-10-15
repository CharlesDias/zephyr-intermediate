/*
 * Lecture 4 - Homework
 *
 * ================================================================
 * Goal: Build a small event-driven system with zbus
 *
 * Tasks:
 *  - Create one zbus channel for sensor data
 *  - Publish simulated sensor samples every 100 ms
 *  - Add one listener for fast display updates
 *  - Add one subscriber for slower logging
 *
 * Push tag l4-task1
 * ================================================================
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_DBG);

#define STACK_SIZE          2048
#define SENSOR_PERIOD_MS    100

struct sensor_data {
    int32_t temperature_mc;
    uint32_t timestamp_ms;
    uint32_t sequence;
};

static void display_listener_callback(const struct zbus_channel *channel);

ZBUS_LISTENER_DEFINE(display_listener, display_listener_callback);

ZBUS_MSG_SUBSCRIBER_DEFINE(logger_subscriber);

ZBUS_CHAN_DEFINE(sensor_channel, struct sensor_data, NULL, NULL,
                 ZBUS_OBSERVERS(display_listener, logger_subscriber),
                 ZBUS_MSG_INIT(.temperature_mc = 0,
                               .timestamp_ms = 0,
                               .sequence = 0));


static void display_listener_callback(const struct zbus_channel *channel)
{
    const struct sensor_data *message = (const struct sensor_data*)zbus_chan_const_msg(channel);

    LOG_INF("[DISPLAY-LIST ] thread = %s sequence = %u temperature = %d mC",
            k_thread_name_get(k_current_get()), message->sequence, message->temperature_mc);
}

static void sensor_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    uint32_t count = 0;

    while(1)
    {
        struct sensor_data data = {
            .temperature_mc = 20000 + (count * 100 % 10000),
            .timestamp_ms   = k_uptime_get_32(),
            .sequence       = count
        };

        count++;

        LOG_INF("[SENSOR-THREAD] publish sequence = %u temperature %d mC",
                data.sequence, data.temperature_mc);

        int ret = zbus_chan_pub(&sensor_channel, &data, K_MSEC(100));
        if(ret != 0) {
            LOG_WRN("[SENSOR-THREAD] publish failed ret=%d", ret);
        }

        k_msleep(SENSOR_PERIOD_MS);
    }
}

static void logger_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    const struct zbus_channel *channel;
    uint32_t received = 0;

    while(1)
    {
        struct sensor_data data;

        int ret = zbus_sub_wait_msg(&logger_subscriber, &channel, &data, K_FOREVER);
        if(ret != 0) {
            LOG_WRN("[LOGGER-THREAD] error ret=%d", ret);
            continue;
        }

        received++;

        LOG_INF("[LOGGER-THREAD] thread = %s sequence = %u temperature = %d mC",
                k_thread_name_get(k_current_get()), data.sequence, data.temperature_mc);
        
        /* Simulates slow logger*/
        k_msleep(50);
    }
}

K_THREAD_DEFINE(sensor_thread, STACK_SIZE, sensor_thread_fn, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(logger_thread, STACK_SIZE, logger_thread_fn, NULL, NULL, NULL, 6, 0, 0);

int main(void)
{
    LOG_INF("=== L4 Homework: Zbus Pub-Sub ===");

    return 0;
}

