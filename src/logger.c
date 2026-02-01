/* Logger_Task.c */

#include "app_common.h"

void logger_task(void *p1, void *p2, void *p3)
{
    while (1) {
        k_mutex_lock(&data_mutex, K_FOREVER);

        printk("\n--- SENSOR DATA ---\n");
        printk("BME280  | T: %.2f C  P: %.2f hPa  H: %.2f %%\n",
               shared_sensor_data.temperature,
               shared_sensor_data.pressure / 100.0,
               shared_sensor_data.humidity);

        // printk("MPU6050 | AX: %.2f AY: %.2f AZ: %.2f\n",
        //        shared_sensor_data.accel_x,
        //        shared_sensor_data.accel_y,
        //        shared_sensor_data.accel_z);

        // printk("        | GX: %.2f GY: %.2f GZ: %.2f\n",
        //        shared_sensor_data.gyro_x,
        //        shared_sensor_data.gyro_y,
        //        shared_sensor_data.gyro_z);

        k_mutex_unlock(&data_mutex);

        k_sleep(K_MSEC(2));
    }
}
