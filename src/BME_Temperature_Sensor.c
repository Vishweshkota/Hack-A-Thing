#include "app_common.h"

SensorData_t  bme_data;


void bme280_task(void *p1, void *p2, void *p3)
{
    const struct device *bme = DEVICE_DT_GET_ANY(bosch_bme280);

    if (!device_is_ready(bme)) {
        printk("BME280 not ready\n");
        return;
    }

    while (1) {
        struct sensor_value temp, press, hum;

        sensor_sample_fetch(bme);
        sensor_channel_get(bme, SENSOR_CHAN_AMBIENT_TEMP, &temp);
        sensor_channel_get(bme, SENSOR_CHAN_PRESS, &press);
        sensor_channel_get(bme, SENSOR_CHAN_HUMIDITY, &hum);

        bme_data.temperature = sensor_value_to_double(&temp);
        // printk("BME280 Temperature: %.2f C\n", bme_data.temperature);
        bme_data.pressure    = sensor_value_to_double(&press);
        // printk("BME280 Pressure: %.2f hPa\n", bme_data.pressure / 100.0);
        bme_data.humidity    = sensor_value_to_double(&hum);
        // printk("BME280 Humidity: %.2f %%\n", bme_data.humidity);
        
        k_mutex_lock(&data_mutex, K_FOREVER);
        shared_sensor_data.temperature = bme_data.temperature;
        shared_sensor_data.pressure    = bme_data.pressure;
        shared_sensor_data.humidity    = bme_data.humidity;
        k_mutex_unlock(&data_mutex);

        k_sleep(K_SECONDS(1));
    }
}
