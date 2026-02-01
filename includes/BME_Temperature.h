#ifndef BME_TEMPERATURE_H
#define BME_TEMPERATURE_H

#define SLEEP_TIME_MS 1000

/* BME280 Register Addresses */
#define REG_ID          0xD0
#define REG_CTRL_HUM    0xF2
#define REG_CTRL_MEAS   0xF4
#define REG_CONFIG      0xF5
#define REG_PRESS_MSB   0xF7
#define REG_CALIB00     0x88    /* Temperature & Pressure calibration */
#define REG_CALIB26     0xE1    /* Humidity calibration */

#define BME280_CHIP_ID  0x60
#define BMP280_CHIP_ID  0x58

/* Get the node identifier of the sensor */
#define I2C_NODE DT_NODELABEL(temperature)

/* Data structure to store BME280 calibration data */
struct bme280_calib_data {
    /* Temperature compensation */
    uint16_t dig_t1;
    int16_t  dig_t2;
    int16_t  dig_t3;
    
    /* Pressure compensation */
    uint16_t dig_p1;
    int16_t  dig_p2;
    int16_t  dig_p3;
    int16_t  dig_p4;
    int16_t  dig_p5;
    int16_t  dig_p6;
    int16_t  dig_p7;
    int16_t  dig_p8;
    int16_t  dig_p9;
    
    /* Humidity compensation */
    uint8_t  dig_h1;
    int16_t  dig_h2;
    uint8_t  dig_h3;
    int16_t  dig_h4;
    int16_t  dig_h5;
    int8_t   dig_h6;
    
    /* Fine temperature for compensation */
    int32_t t_fine;
};

/* Function prototypes */
int32_t bme280_read_calibration(const struct i2c_dt_spec *);
int32_t bme280_compensate_temp(int32_t);
uint32_t bme280_compensate_press(int32_t);
uint32_t bme280_compensate_humidity(int32_t);



#endif /* BME_TEMPERATURE_H */