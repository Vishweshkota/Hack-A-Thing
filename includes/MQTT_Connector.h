#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/random/random.h>
#include <zephyr/drivers/gpio.h>
#include <net/mqtt_helper.h>
#include <zephyr/random/random.h>
#include <stdio.h>
#include <string.h>

/* ================= CONFIG ================= */

#define WIFI_SSID          "NRF"
#define WIFI_PSK           "123456789"

#define MQTT_BROKER_ADDR   "10.110.140.200"
#define MQTT_BROKER_PORT   1883
#define MQTT_CLIENT_ID     "nrf7002_belt"

#define TOPIC_PUB          "belt/sensors"
#define TOPIC_SUB          "belt/commands"

#define PUBLISH_INTERVAL_MS 5000