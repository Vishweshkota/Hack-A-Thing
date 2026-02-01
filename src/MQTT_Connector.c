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

/* LED */
#define LED0_NODE DT_ALIAS(led0)
#if DT_NODE_EXISTS(LED0_NODE)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#endif

/* ═══════════════════════════════════════════════════════════════════════════
   COMMAND HANDLER
   ═══════════════════════════════════════════════════════════════════════════ */
static void handle_command(const char *cmd, size_t len)
{
    LOG_INF("CMD: %.*s", (int)len, cmd);

#if DT_NODE_EXISTS(LED0_NODE)
    if (!device_is_ready(led.port)) {
        return;
    }

    if (strncmp(cmd, "LED_ON", 6) == 0) {
        gpio_pin_set_dt(&led, 1);
        LOG_INF("LED ON");
    } else if (strncmp(cmd, "LED_OFF", 7) == 0) {
        gpio_pin_set_dt(&led, 0);
        LOG_INF("LED OFF");
    }
#endif
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
   WIFI CONNECT
   ═══════════════════════════════════════════════════════════════════════════ */
static int wifi_args_to_params(struct wifi_connect_req_params *params)
{
    params->ssid = CONFIG_WIFI_CREDENTIALS_STATIC_SSID;
    params->ssid_length = strlen(params->ssid);
    params->psk = CONFIG_WIFI_CREDENTIALS_STATIC_PASSWORD;
    params->psk_length = strlen(params->psk);
    params->channel = WIFI_CHANNEL_ANY;
    params->security = WIFI_SECURITY_TYPE_PSK;
    params->mfp = WIFI_MFP_OPTIONAL;
    params->timeout = SYS_FOREVER_MS;
    params->band = WIFI_FREQ_BAND_UNKNOWN;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
   MQTT TASK
   ═══════════════════════════════════════════════════════════════════════════ */
/* ═══════════════════════════════════════════════════════════════════════════
   MQTT TASK
   ═══════════════════════════════════════════════════════════════════════════ */
void mqtt_task(void *arg1, void *arg2, void *arg3)
{
    int err;
    int counter = 0;
    char msg[256];

    LOG_INF("════════════════════════════════════════");
    LOG_INF("  MQTT Task Started");
    LOG_INF("════════════════════════════════════════");

    /* Init LED */
#if DT_NODE_EXISTS(LED0_NODE)
    if (device_is_ready(led.port)) {
        gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
        LOG_INF("LED initialized");
    }
#endif

    /* Wait for WiFi driver init */
    k_sleep(K_SECONDS(1));

    /* Register network event callbacks */
    net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler, EVENT_MASK);
    net_mgmt_add_event_callback(&mgmt_cb);

    /* WiFi auto-connect is enabled - just wait for connection */
    LOG_INF("Waiting for WiFi (auto-connect)...");
    k_sem_take(&wifi_connected_sem, K_FOREVER);
    
    LOG_INF("WiFi connected! Starting MQTT...");

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

    /* Connect to MQTT broker */
    struct mqtt_helper_conn_params conn_params = {
        .hostname.ptr = CONFIG_MQTT_SAMPLE_BROKER_HOSTNAME,
        .hostname.size = strlen(CONFIG_MQTT_SAMPLE_BROKER_HOSTNAME),
        .device_id.ptr = client_id,
        .device_id.size = strlen(client_id),
    };

    LOG_INF("Connecting to MQTT: %s", CONFIG_MQTT_SAMPLE_BROKER_HOSTNAME);
    err = mqtt_helper_connect(&conn_params);
    if (err) {
        LOG_ERR("MQTT connect failed: %d", err);
        return;
    }

    /* Wait for MQTT connection */
    for (int i = 0; i < 100 && !mqtt_connected; i++) {
        k_msleep(100);
    }

    if (!mqtt_connected) {
        LOG_ERR("MQTT connection timeout!");
        return;
    }

    LOG_INF("════════════════════════════════════════");
    LOG_INF("  MQTT Ready - Publishing Data");
    LOG_INF("════════════════════════════════════════");

    /* Main loop - publish sensor data */
    while (1) {
        if (mqtt_connected) {
            snprintf(msg, sizeof(msg),
                "{\"id\":%d,\"device\":\"nRF7002-Belt\",\"temp\":25.5,\"humidity\":60.0}",
                counter++);

            err = mqtt_publish_msg(msg);
            if (err == 0) {
                LOG_INF("Published [%d]", counter - 1);
            } else {
                LOG_ERR("Publish failed (%d)", err);
            }
        }

        k_msleep(PUBLISH_INTERVAL_MS);
    }
}