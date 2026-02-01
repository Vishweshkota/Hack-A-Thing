// #ifndef COMMON_H
// #define COMMON_H

// #include <zephyr/kernel.h>
// #include <zephyr/device.h>
// #include <zephyr/devicetree.h>
// #include <zephyr/sys/printk.h>
// #include <zephyr/drivers/i2c.h>
// #include <zephyr/drivers/sensor.h> 
// #include <zephyr/drivers/gpio.h>
// #include <zephyr/drivers/pwm.h>

// /* Devicetree nodes */
// #define BME280_NODE   DT_NODELABEL(temperature)
// #define MPU6050_NODE  DT_NODELABEL(accelerometer)

// static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

// /* Defining Stack size for sensors*/
// #define BME280_STACK_SIZE  1024
// #define MPU6050_STACK_SIZE 1024
// #define ACCUATOR_STACK_SIZE 1024
// #define MQTT_STACK_SIZE    8129

// #define LOGGER_STACK_SIZE 1024

// #define BME280_PRIORITY 5
// #define MPU6050_PRIORITY 5
// #define ACCUATOR_PRIORITY 5
// #define MQTT_PRIORITY 6

// /* Shared data structure */
// typedef struct{
//     /* BME280 data */
//     double temperature;
//     double pressure;
//     double humidity;
    
//     /* MPU6050 data */
//     double accel_x;
//     double accel_y;
//     double accel_z;
//     double gyro_x;
//     double gyro_y;
//     double gyro_z;

//     float accel_magnitude;
    
//     /* Flags */
//     bool fall_detected;
//     bool high_temp_alert;
// }SensorData_t;

// typedef struct{
//     bool ledON;
//     bool motorVibrate;
//     bool buzzerON;
// }ActuatorData_t;

// /* Global shared data (protected by mutex) */
// extern struct k_mutex data_mutex;
// extern struct k_mutex actuator_mutex;

// extern SensorData_t shared_sensor_data;
// extern ActuatorData_t shared_actuator_data;

// #ifndef ARCH_STACK_PTR_ALIGN
// #define ARCH_STACK_PTR_ALIGN 4 /* Define a default alignment if not already defined */
// #endif

// K_THREAD_STACK_DECLARE(bme_stack, BME280_STACK_SIZE);
// K_THREAD_STACK_DECLARE(mpu_stack, MPU6050_STACK_SIZE);
// K_THREAD_STACK_DECLARE(MQTT_stack, MQTT_STACK_SIZE);

// K_THREAD_STACK_DECLARE(logger_stack, LOGGER_STACK_SIZE);

// // extern struct k_thread bme_thread;
// // extern struct k_thread mpu_thread;
// // extern struct k_thread MQTT_thread;

// // extern struct k_thread logger_thread;


// /* Function prototypes */
// void bme280_task(void *, void *, void *);
// void mpu6050_task(void *, void *, void *);
// void actuator_task(void *p1, void *p2, void *p3);
// void mqtt_task(void *, void *, void *);


// #endif /* COMMON_H */

#ifndef COMMON_H
#define COMMON_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h> 
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>

/* ══════════════════════════════════════════════════════════════
   DO NOT PUT GPIO_DT_SPEC_GET OR PWM_DT_SPEC_GET HERE!
   ══════════════════════════════════════════════════════════════ */

/* Devicetree node labels (these are OK in headers) */
#define BME280_NODE   DT_NODELABEL(temperature)
#define MPU6050_NODE  DT_NODELABEL(accelerometer)

/* Stack sizes */
#define BME280_STACK_SIZE   2048
#define MPU6050_STACK_SIZE  2048
#define ACTUATOR_STACK_SIZE 2048
#define MQTT_STACK_SIZE     8192

/* Priorities */
#define BME280_PRIORITY   5
#define MPU6050_PRIORITY  5
#define ACTUATOR_PRIORITY 5
#define MQTT_PRIORITY     6

/* Sensor Data Structure */
typedef struct {
    double temperature;
    double pressure;
    double humidity;
    double accel_x;
    double accel_y;
    double accel_z;
    double gyro_x;
    double gyro_y;
    double gyro_z;
    float accel_magnitude;
    bool fall_detected;
    bool high_temp_alert;
} SensorData_t;

/* Actuator Data Structure */
typedef struct {
    bool ledON;
    bool motorVibrate;
    bool buzzerON;
} ActuatorData_t;

/* Global shared data (extern) */
extern struct k_mutex data_mutex;
extern struct k_mutex actuator_mutex;
extern SensorData_t shared_sensor_data;
extern ActuatorData_t shared_actuator_data;

/* Task prototypes */
void bme280_task(void *p1, void *p2, void *p3);
void mpu6050_task(void *p1, void *p2, void *p3);
void actuator_task(void *p1, void *p2, void *p3);
void mqtt_task(void *p1, void *p2, void *p3);


#endif /* COMMON_H */