#include "MQTT_Connector.h"
#include "app_common.h"
#include <net/mqtt_helper.h>
#include <zephyr/random/random.h>

LOG_MODULE_REGISTER(wifi_mqtt, LOG_LEVEL_INF);

/* ═══════════════════════════════════════════════════════════════════════════
   VARIABLES
   ═══════════════════════════════════════════════════════════════════════════ */
static bool wifi_connected;
static bool mqtt_connected;

static struct net_mgmt_event_callback mgmt_cb;
static K_SEM_DEFINE(wifi_connected_sem, 0, 1);

static uint8_t client_id[32];

#define SUBSCRIBE_TOPIC_ID 1234


/* ═══════════════════════════════════════════════════════════════════════════
   COMMAND HANDLER
   ═══════════════════════════════════════════════════════════════════════════ */
static void handle_command(const char *cmd, size_t len)
{
    LOG_INF("CMD Received: %.*s", (int)len, cmd);

    /* Lock mutex before updating shared actuator data */
    k_mutex_lock(&actuator_mutex, K_FOREVER);

    if (strncmp(cmd, "LED_ON", 6) == 0) {
        shared_actuator_data.ledON = true;
        LOG_INF("LED ON command set");
    } 
    else if (strncmp(cmd, "LED_OFF", 7) == 0) {
        shared_actuator_data.ledON = false;
        LOG_INF("LED OFF command set");
    }
    else if (strncmp(cmd, "MOTOR_ON", 8) == 0) {
        shared_actuator_data.motorVibrate = true;
        LOG_INF("MOTOR ON command set");
    }
    else if (strncmp(cmd, "MOTOR_OFF", 9) == 0) {
        shared_actuator_data.motorVibrate = false;
        LOG_INF("MOTOR OFF command set");
    }
    else if (strncmp(cmd, "BUZZER_ON", 9) == 0) {
        shared_actuator_data.buzzerON = true;
        LOG_INF("BUZZER ON command set");
    }
    else if (strncmp(cmd, "BUZZER_OFF", 10) == 0) {
        shared_actuator_data.buzzerON = false;
        LOG_INF("BUZZER OFF command set");
    }
    else {
        LOG_WRN("Unknown command: %.*s", (int)len, cmd);
    }

    k_mutex_unlock(&actuator_mutex);
}

/* ═══════════════════════════════════════════════════════════════════════════
   WIFI EVENTS
   ═══════════════════════════════════════════════════════════════════════════ */
#define EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                   uint64_t mgmt_event, struct net_if *iface)
{
    if ((mgmt_event & EVENT_MASK) != mgmt_event) {
        return;
    }

    if (mgmt_event == NET_EVENT_L4_CONNECTED) {
        LOG_INF("Network connected");
        wifi_connected = true;
        k_sem_give(&wifi_connected_sem);
        return;
    }

    if (mgmt_event == NET_EVENT_L4_DISCONNECTED) {
        if (wifi_connected == false) {
            LOG_INF("Waiting for network to be connected");
        } else {
            LOG_INF("Network disconnected");
            wifi_connected = false;
            mqtt_connected = false;
            mqtt_helper_disconnect();
        }
        k_sem_reset(&wifi_connected_sem);
        return;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   MQTT CALLBACKS
   ═══════════════════════════════════════════════════════════════════════════ */
static void subscribe(void)
{
    struct mqtt_topic subscribe_topic = {
        .topic = {
            .utf8 = CONFIG_MQTT_SAMPLE_SUB_TOPIC,
            .size = strlen(CONFIG_MQTT_SAMPLE_SUB_TOPIC)
        },
        .qos = MQTT_QOS_1_AT_LEAST_ONCE
    };

    struct mqtt_subscription_list subscription_list = {
        .list = &subscribe_topic,
        .list_count = 1,
        .message_id = SUBSCRIBE_TOPIC_ID
    };

    LOG_INF("Subscribing to %s", CONFIG_MQTT_SAMPLE_SUB_TOPIC);
    int err = mqtt_helper_subscribe(&subscription_list);
    if (err) {
        LOG_ERR("Failed to subscribe, error: %d", err);
    }
}

static void on_mqtt_connack(enum mqtt_conn_return_code return_code, bool session_present)
{
    if (return_code == MQTT_CONNECTION_ACCEPTED) {
        LOG_INF("MQTT: Connected to broker!");
        LOG_INF("  Broker: %s", CONFIG_MQTT_SAMPLE_BROKER_HOSTNAME);
        LOG_INF("  Client: %s", client_id);
        mqtt_connected = true;
        subscribe();
    } else {
        LOG_ERR("MQTT: Connection failed, code: %d", return_code);
    }
}

static void on_mqtt_disconnect(int result)
{
    LOG_INF("MQTT: Disconnected (%d)", result);
    mqtt_connected = false;
}

static void on_mqtt_publish(struct mqtt_helper_buf topic, struct mqtt_helper_buf payload)
{
    LOG_INF("MQTT RX: %.*s", payload.size, payload.ptr);
    handle_command(payload.ptr, payload.size);
}

static void on_mqtt_suback(uint16_t message_id, int result)
{
    if (result != MQTT_SUBACK_FAILURE) {
        LOG_INF("MQTT: Subscribed (QoS %d)", result);
    } else {
        LOG_ERR("MQTT: Subscribe failed");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   PUBLISH FUNCTION
   ═══════════════════════════════════════════════════════════════════════════ */
static int mqtt_publish_msg(const char *payload)
{
    if (!mqtt_connected) {
        return -ENOTCONN;
    }

    struct mqtt_publish_param param = {
        .message.payload.data = (uint8_t *)payload,
        .message.payload.len = strlen(payload),
        .message.topic.qos = MQTT_QOS_0_AT_MOST_ONCE,
        .message_id = mqtt_helper_msg_id_get(),
        .message.topic.topic.utf8 = CONFIG_MQTT_SAMPLE_PUB_TOPIC,
        .message.topic.topic.size = strlen(CONFIG_MQTT_SAMPLE_PUB_TOPIC),
        .dup_flag = 0,
        .retain_flag = 0,
    };

    return mqtt_helper_publish(&param);
}


/* ═══════════════════════════════════════════════════════════════════════════
   MQTT TASK
   ═══════════════════════════════════════════════════════════════════════════ */
/* ═══════════════════════════════════════════════════════════════════════════
   MQTT TASK - With Retry Mechanism
   ═══════════════════════════════════════════════════════════════════════════ */
void mqtt_task(void *arg1, void *arg2, void *arg3)
{
    int err;
    int counter = 0;
    char msg[512];
    int mqtt_retry_count = 0;
    const int MAX_MQTT_RETRIES = 10;
    const int MQTT_RETRY_DELAY_MS = 5000;

    LOG_INF("════════════════════════════════════════");
    LOG_INF("  MQTT Task Started");
    LOG_INF("════════════════════════════════════════");

    /* Wait for WiFi driver init */
    k_sleep(K_SECONDS(1));

    /* Register network event callbacks */
    net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler, EVENT_MASK);
    net_mgmt_add_event_callback(&mgmt_cb);

    /* WiFi auto-connect is enabled - just wait for connection */
    LOG_INF("Waiting for WiFi (auto-connect)...");
    k_sem_take(&wifi_connected_sem, K_FOREVER);
    
    LOG_INF("WiFi connected! Waiting for IP...");
    k_sleep(K_SECONDS(3));  /* Wait for DHCP */

    /* Initialize MQTT Helper */
    struct mqtt_helper_cfg config = {
        .cb = {
            .on_connack = on_mqtt_connack,
            .on_disconnect = on_mqtt_disconnect,
            .on_publish = on_mqtt_publish,
            .on_suback = on_mqtt_suback,
        },
    };

    err = mqtt_helper_init(&config);
    if (err) {
        LOG_ERR("MQTT helper init failed: %d", err);
        return;
    }
    LOG_INF("MQTT Helper initialized");

    /* Generate client ID */
    uint32_t id = sys_rand32_get();
    snprintf(client_id, sizeof(client_id), "nrf7002-belt-%08x", id);

    /* MQTT connection params */
    struct mqtt_helper_conn_params conn_params = {
        .hostname.ptr = CONFIG_MQTT_SAMPLE_BROKER_HOSTNAME,
        .hostname.size = strlen(CONFIG_MQTT_SAMPLE_BROKER_HOSTNAME),
        .device_id.ptr = client_id,
        .device_id.size = strlen(client_id),
    };

    /* ═══════════════════════════════════════════════════════════════════════
       MQTT CONNECT WITH RETRY
       ═══════════════════════════════════════════════════════════════════════ */
    while (!mqtt_connected && mqtt_retry_count < MAX_MQTT_RETRIES) {
        LOG_INF("Connecting to MQTT: %s (attempt %d/%d)", 
                CONFIG_MQTT_SAMPLE_BROKER_HOSTNAME,
                mqtt_retry_count + 1, 
                MAX_MQTT_RETRIES);
        
        err = mqtt_helper_connect(&conn_params);
        if (err) {
            LOG_ERR("MQTT connect request failed: %d", err);
            mqtt_retry_count++;
            LOG_INF("Retrying in %d seconds...", MQTT_RETRY_DELAY_MS / 1000);
            k_msleep(MQTT_RETRY_DELAY_MS);
            continue;
        }

        /* Wait for CONNACK */
        for (int i = 0; i < 100 && !mqtt_connected; i++) {
            k_msleep(100);
        }

        if (!mqtt_connected) {
            LOG_WRN("MQTT connection timeout, retrying...");
            mqtt_helper_disconnect();
            mqtt_retry_count++;
            k_msleep(MQTT_RETRY_DELAY_MS);
        }
    }

    if (!mqtt_connected) {
        LOG_ERR("MQTT connection failed after %d attempts!", MAX_MQTT_RETRIES);
        LOG_ERR("Continuing without MQTT - will retry in main loop");
    } else {
        LOG_INF("════════════════════════════════════════");
        LOG_INF("  MQTT Connected! Publishing Data");
        LOG_INF("  Topic: %s", CONFIG_MQTT_SAMPLE_PUB_TOPIC);
        LOG_INF("════════════════════════════════════════");
    }

    /* ═══════════════════════════════════════════════════════════════════════
       MAIN LOOP - With Reconnection
       ═══════════════════════════════════════════════════════════════════════ */
    while (1) {
        /* Check WiFi connection */
        if (!wifi_connected) {
            LOG_WRN("WiFi disconnected, waiting for reconnect...");
            k_sem_take(&wifi_connected_sem, K_FOREVER);
            LOG_INF("WiFi reconnected!");
            k_sleep(K_SECONDS(3));  /* Wait for DHCP */
            mqtt_connected = false;  /* Force MQTT reconnect */
        }

        /* Reconnect MQTT if disconnected */
        if (!mqtt_connected && wifi_connected) {
            LOG_INF("MQTT disconnected, reconnecting...");
            k_msleep(2000);
            
            err = mqtt_helper_connect(&conn_params);
            if (err) {
                LOG_ERR("MQTT reconnect failed: %d", err);
                k_msleep(MQTT_RETRY_DELAY_MS);
                continue;
            }

            /* Wait for CONNACK */
            for (int i = 0; i < 100 && !mqtt_connected; i++) {
                k_msleep(100);
            }

            if (mqtt_connected) {
                LOG_INF("MQTT reconnected!");
            }
            continue;
        }

        /* Publish sensor data */
        if (mqtt_connected) {
            /* Get sensor data with mutex protection */
            SensorData_t data;
            k_mutex_lock(&data_mutex, K_FOREVER);
            memcpy(&data, &shared_sensor_data, sizeof(data));
            k_mutex_unlock(&data_mutex);

            /* Format JSON with real sensor data */
            snprintf(msg, sizeof(msg),
                "{"
                "\"packetid\":%d,"
                "\"device\":\"nRF7002-Belt\","
                "\"temperature\":%.2f,"
                "\"humidity\":%.2f,"
                "\"pressure\":%.2f,"
                "\"accel_x\":%.2f,"
                "\"accel_y\":%.2f,"
                "\"accel_z\":%.2f,"
                "\"gyro_x\":%.2f,"
                "\"gyro_y\":%.2f,"
                "\"gyro_z\":%.2f,"
                "\"fall_detected\":%s"
                "}",
                counter++,
                data.temperature,
                data.humidity,
                data.pressure,
                data.accel_x,
                data.accel_y,
                data.accel_z,
                data.gyro_x,
                data.gyro_y,
                data.gyro_z,
                data.fall_detected ? "true" : "false"
            );

            err = mqtt_publish_msg(msg);
            if (err == 0) {
                LOG_INF("Published [%d]: T=%.1fC A=(%.1f,%.1f,%.1f)", 
                        counter - 1, 
                        data.temperature,
                        data.accel_x, data.accel_y, data.accel_z);
            } else {
                LOG_ERR("Publish failed (%d)", err);
                if (err == -ENOTCONN) {
                    mqtt_connected = false;  /* Trigger reconnect */
                }
            }
        }

        k_msleep(PUBLISH_INTERVAL_MS);
    }
}