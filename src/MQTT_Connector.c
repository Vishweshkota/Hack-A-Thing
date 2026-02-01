#include "MQTT_Connector.h"
#include "app_common.h"

LOG_MODULE_REGISTER(wifi_mqtt, LOG_LEVEL_INF);

/* ═══════════════════════════════════════════════════════════════════════════
   GLOBAL VARIABLES
   ═══════════════════════════════════════════════════════════════════════════ */
static bool wifi_connected;
static bool mqtt_connected;

static struct mqtt_client client;
static uint8_t rx_buffer[256];
static uint8_t tx_buffer[256];
static struct sockaddr_storage broker;

static struct net_mgmt_event_callback mgmt_cb;

/* LED */
#define LED0_NODE DT_ALIAS(led0)
#if DT_NODE_EXISTS(LED0_NODE)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#endif

/* Semaphore to wait for connection */
static K_SEM_DEFINE(wifi_connected_sem, 0, 1);

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
   WIFI EVENTS - Using Nordic's L4 pattern
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
        }
        k_sem_reset(&wifi_connected_sem);
        return;
    }
}

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
   MQTT FUNCTIONS
   ═══════════════════════════════════════════════════════════════════════════ */

static void mqtt_evt_handler(struct mqtt_client *c, const struct mqtt_evt *evt)
{
    switch (evt->type) {
    case MQTT_EVT_CONNACK:
        if (evt->result == 0) {
            LOG_INF("MQTT: Connected!");
            mqtt_connected = true;
        } else {
            LOG_ERR("MQTT: Connect failed (%d)", evt->result);
        }
        break;

    case MQTT_EVT_DISCONNECT:
        LOG_INF("MQTT: Disconnected");
        mqtt_connected = false;
        break;

    case MQTT_EVT_PUBLISH: {
        const struct mqtt_publish_param *p = &evt->param.publish;
        uint8_t buf[128];
        size_t len = MIN(p->message.payload.len, sizeof(buf) - 1);

        mqtt_read_publish_payload_blocking(c, buf, len);
        buf[len] = '\0';

        handle_command((char *)buf, len);

        if (p->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
            struct mqtt_puback_param ack = { .message_id = p->message_id };
            mqtt_publish_qos1_ack(c, &ack);
        }
        break;
    }

    case MQTT_EVT_SUBACK:
        LOG_INF("MQTT: Subscribed!");
        break;

    default:
        break;
    }
}

static int mqtt_broker_init(void)
{
    struct sockaddr_in *b = (struct sockaddr_in *)&broker;
    struct zsock_addrinfo *res;
    struct zsock_addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM
    };

    LOG_INF("DNS: Resolving %s...", MQTT_BROKER_ADDR);

    int ret = zsock_getaddrinfo(MQTT_BROKER_ADDR, NULL, &hints, &res);
    if (ret) {
        LOG_ERR("DNS failed (%d)", ret);
        return ret;
    }

    b->sin_family = AF_INET;
    b->sin_port = htons(MQTT_BROKER_PORT);
    b->sin_addr.s_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr;

    zsock_freeaddrinfo(res);
    LOG_INF("MQTT broker: %s:%d", MQTT_BROKER_ADDR, MQTT_BROKER_PORT);
    return 0;
}

static int mqtt_connect_client(void)
{
    mqtt_client_init(&client);

    client.broker = (struct sockaddr *)&broker;
    client.evt_cb = mqtt_evt_handler;
    client.client_id.utf8 = (uint8_t *)MQTT_CLIENT_ID;
    client.client_id.size = strlen(MQTT_CLIENT_ID);
    client.protocol_version = MQTT_VERSION_3_1_1;
    client.keepalive = 60;

    client.rx_buf = rx_buffer;
    client.rx_buf_size = sizeof(rx_buffer);
    client.tx_buf = tx_buffer;
    client.tx_buf_size = sizeof(tx_buffer);

    client.transport.type = MQTT_TRANSPORT_NON_SECURE;

    LOG_INF("MQTT: Connecting...");
    return mqtt_connect(&client);
}

static int mqtt_subscribe_topic(void)
{
    struct mqtt_topic topic = {
        .topic.utf8 = (uint8_t *)TOPIC_SUB,
        .topic.size = strlen(TOPIC_SUB),
        .qos = MQTT_QOS_1_AT_LEAST_ONCE
    };

    struct mqtt_subscription_list sub = {
        .list = &topic,
        .list_count = 1,
        .message_id = sys_rand32_get()
    };

    LOG_INF("MQTT: Subscribing to %s", TOPIC_SUB);
    return mqtt_subscribe(&client, &sub);
}

static int mqtt_publish_msg(const char *payload)
{
    if (!mqtt_connected) {
        return -ENOTCONN;
    }

    struct mqtt_publish_param p = {
        .message.topic.topic.utf8 = (uint8_t *)TOPIC_PUB,
        .message.topic.topic.size = strlen(TOPIC_PUB),
        .message.payload.data = (uint8_t *)payload,
        .message.payload.len = strlen(payload),
        .message.topic.qos = MQTT_QOS_0_AT_MOST_ONCE,
        .message_id = sys_rand32_get()
    };

    return mqtt_publish(&client, &p);
}

/* ═══════════════════════════════════════════════════════════════════════════
   MQTT TASK - Using Nordic's pattern
   ═══════════════════════════════════════════════════════════════════════════ */

void mqtt_task(void *arg1, void *arg2, void *arg3)
{
    int counter = 0;
    char msg[256];
    int ret;

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

    /* Sleep to allow initialization of Wi-Fi driver */
    k_sleep(K_SECONDS(1));

    /* Initialize and add the callback function for network events */
    net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler, EVENT_MASK);
    net_mgmt_add_event_callback(&mgmt_cb);

    /* Get the network interface */
    struct net_if *iface = net_if_get_first_wifi();
    if (iface == NULL) {
        LOG_ERR("No WiFi interface found!");
        return;
    }
    LOG_INF("WiFi interface ready");

    /* Populate connection parameters */
    struct wifi_connect_req_params cnx_params;
    wifi_args_to_params(&cnx_params);

    /* Request WiFi connection */
    LOG_INF("Connecting to WiFi: %s", cnx_params.ssid);
    ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &cnx_params, sizeof(cnx_params));
    if (ret) {
        LOG_ERR("WiFi connect failed (%d)", ret);
        return;
    }

    /* Wait for connection (with timeout) */
    LOG_INF("Waiting for WiFi connection...");
    ret = k_sem_take(&wifi_connected_sem, K_SECONDS(30));
    if (ret) {
        LOG_ERR("WiFi connection timeout!");
        return;
    }

    /* Wait for IP address (DHCP) */
    LOG_INF("Waiting for IP address...");
    k_msleep(3000);

    /* Init MQTT broker */
    ret = mqtt_broker_init();
    if (ret) {
        LOG_ERR("MQTT broker init failed");
        return;
    }

    /* Connect to MQTT broker */
    ret = mqtt_connect_client();
    if (ret) {
        LOG_ERR("MQTT connect failed (%d)", ret);
        return;
    }

    /* Wait for MQTT CONNACK */
    LOG_INF("Waiting for MQTT connection...");
    for (int i = 0; i < 50 && !mqtt_connected; i++) {
        mqtt_input(&client);
        k_msleep(100);
    }

    if (!mqtt_connected) {
        LOG_ERR("MQTT connection timeout!");
        return;
    }

    /* Subscribe to commands topic */
    mqtt_subscribe_topic();

    LOG_INF("════════════════════════════════════════");
    LOG_INF("  MQTT Ready - Publishing Data");
    LOG_INF("════════════════════════════════════════");

    /* Main loop */
    while (1) {
        mqtt_input(&client);
        mqtt_live(&client);

        if (!mqtt_connected && wifi_connected) {
            LOG_WRN("MQTT disconnected, reconnecting...");
            k_msleep(2000);
            mqtt_connect_client();
            for (int i = 0; i < 30 && !mqtt_connected; i++) {
                mqtt_input(&client);
                k_msleep(100);
            }
            if (mqtt_connected) {
                mqtt_subscribe_topic();
            }
            continue;
        }

        if (mqtt_connected) {
            snprintf(msg, sizeof(msg),
                "{\"id\":%d,\"device\":\"nRF7002-Belt\",\"temp\":25.5,\"humidity\":60.0}",
                counter++);

            ret = mqtt_publish_msg(msg);
            if (ret == 0) {
                LOG_INF("Published [%d]", counter - 1);
            } else {
                LOG_ERR("Publish failed (%d)", ret);
            }
        }

        k_msleep(PUBLISH_INTERVAL_MS);
    }
}