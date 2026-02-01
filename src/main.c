// /* Main file to run the application*/

// # include "common.h"


// int main()
// {
//     startBMESensor();
//     startMPU6050Sensor();
//     return 0;
// }
#include "app_common.h"

K_THREAD_STACK_DEFINE(bme_stack, BME280_STACK_SIZE);
K_THREAD_STACK_DEFINE(mpu_stack, MPU6050_STACK_SIZE);
K_THREAD_STACK_DEFINE(mqtt_stack, MQTT_STACK_SIZE);

K_THREAD_STACK_DEFINE(logger_stack, LOGGER_STACK_SIZE);

/* Thread objects */
struct k_thread bme_thread;
struct k_thread mpu_thread;
struct k_thread mqtt_thread;

struct k_thread logger_thread;

/* Mutex for protecting shared data */
struct k_mutex data_mutex;
SensorData_t shared_sensor_data;

int main(void)
{
    printk("=== Sensor Tasks Starting ===\n");

    k_mutex_init(&data_mutex);

    k_thread_create(&mqtt_thread,
                    mqtt_stack,
                    K_THREAD_STACK_SIZEOF(mqtt_stack),
                    mqtt_task,
                    NULL, NULL, NULL,
                    MQTT_PRIORITY, 0, K_NO_WAIT);

    k_thread_create(&bme_thread,
                    bme_stack,
                    K_THREAD_STACK_SIZEOF(bme_stack),
                    bme280_task,
                    NULL, NULL, NULL,
                    BME280_PRIORITY, 0, K_NO_WAIT);

    k_thread_create(&mpu_thread,
                    mpu_stack,
                    K_THREAD_STACK_SIZEOF(mpu_stack),
                    mpu6050_task,
                    NULL, NULL, NULL,
                    MPU6050_PRIORITY, 0, K_NO_WAIT); 
        
    // k_thread_create(&logger_thread,
    //                 logger_stack,
    //                 K_THREAD_STACK_SIZEOF(logger_stack),
    //                 logger_task,
    //                 NULL, NULL, NULL,
    //                 7, 0, K_NO_WAIT); /* lower priority */

    return 0;
}
