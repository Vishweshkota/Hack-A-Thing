#ifndef ACCCEL_MPU6050_H
#define ACCCEL_MPU6050_H

#include "common.h"
/* Registers */
#define MPU6050_REG_PWR_MGMT_1      0x6B
#define MPU6050_REG_GYRO_CONFIG     0x1B
#define MPU6050_REG_ACCEL_CONFIG    0x1C
#define MPU6050_REG_ACCEL_XOUT_H    0x3B
#define MPU6050_REG_WHO_AM_I        0x75



int mpu6050_init(const struct i2c_dt_spec *dev);
int mpu6050_read_temp(const struct i2c_dt_spec *dev, float *temp);
#endif /* ACCCEL_MPU6050_H */