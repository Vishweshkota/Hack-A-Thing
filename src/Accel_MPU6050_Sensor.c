#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include "app_common.h"

SensorData_t imu_data;


void mpu6050_task(void *p1, void *p2, void *p3)
{
    const struct device *imu = DEVICE_DT_GET_ANY(invensense_mpu6050);
    struct sensor_value accel[3], gyro[3];

    if (!device_is_ready(imu)) {
        printk("MPU6050 not ready\n");
        return;
    }

    while (1) {

        sensor_sample_fetch(imu);
        sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, accel);
        sensor_channel_get(imu, SENSOR_CHAN_GYRO_XYZ, gyro);

        imu_data.accel_x = sensor_value_to_double(&accel[0]);
        imu_data.accel_y = sensor_value_to_double(&accel[1]);
        imu_data.accel_z = sensor_value_to_double(&accel[2]);
        
        imu_data.gyro_x = sensor_value_to_double(&gyro[0]);
        imu_data.gyro_y = sensor_value_to_double(&gyro[1]);
        imu_data.gyro_z = sensor_value_to_double(&gyro[2]);
        
        k_mutex_lock(&data_mutex, K_FOREVER);
        shared_sensor_data.accel_x = imu_data.accel_x;
        shared_sensor_data.accel_y = imu_data.accel_y;
        shared_sensor_data.accel_z = imu_data.accel_z;
        k_mutex_unlock(&data_mutex);

        k_sleep(K_MSEC(50));  /* 20 Hz */
    }
}
