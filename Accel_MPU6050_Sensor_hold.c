// /* File to handle MPU6050 related tasks*/

// //  * MPU6050 Accelerometer and Gyroscope Sensor

// #include "common.h"
// #include "Accel_MPU6050.h"

// void startMPU6050Sensor()
// {
//     const struct device *mpu = DEVICE_DT_GET(MPU6050_NODE);
//     struct sensor_value accel[3], gyro[3], temp;

//     printk("\nMPU6050 - Zephyr Driver\n\n");

//     if (!device_is_ready(mpu)) {
//         printk("MPU6050 not ready!\n");
//         return;
//     }

//     printk("MPU6050 ready!\n\n");

//     while (1) {
//         /* Fetch all data */
//         sensor_sample_fetch(mpu);

//         /* Get accelerometer */
//         sensor_channel_get(mpu, SENSOR_CHAN_ACCEL_XYZ, accel);

//         /* Get gyroscope */
//         sensor_channel_get(mpu, SENSOR_CHAN_GYRO_XYZ, gyro);

//         /* Get temperature */
//         sensor_channel_get(mpu, SENSOR_CHAN_DIE_TEMP, &temp);

//         /* Print values */
//         printk("Accel: X=%d.%06d Y=%d.%06d Z=%d.%06d m/s²\n",
//                accel[0].val1, accel[0].val2,
//                accel[1].val1, accel[1].val2,
//                accel[2].val1, accel[2].val2);

//         printk("Gyro:  X=%d.%06d Y=%d.%06d Z=%d.%06d rad/s\n",
//                gyro[0].val1, gyro[0].val2,
//                gyro[1].val1, gyro[1].val2,
//                gyro[2].val1, gyro[2].val2);

//         printk("Temp:  %d.%06d °C\n\n",
//                temp.val1, temp.val2);

//         k_msleep(500);
//     }

//     return 0;
// }

