// /* File to handle BME280 related tasks
//  * BME280 Temperature, Pressure, and Humidity Sensor
//  */

// #include "common.h"
// #include "BME_Temperature.h"

// SensorData_t shared_sensor_data;
// struct k_mutex sensor_data_mutex;

// /* Function prototype for bme280_read_calibration */

// static struct bme280_calib_data calib;
// /* Read calibration data from BME280 */
// int32_t bme280_read_calibration(const struct i2c_dt_spec *dev)
// {
//     uint8_t buf[26];
//     int ret;

//     /* Read temperature and pressure calibration (0x88 - 0xA1) */
//     ret = i2c_burst_read_dt(dev, REG_CALIB00, buf, 26);
//     if (ret != 0) {
//         printk("Failed to read calibration data\n");
//         return ret;
//     }

//     /* Parse temperature calibration */
//     calib.dig_t1 = (uint16_t)(buf[1] << 8) | buf[0];
//     calib.dig_t2 = (int16_t)(buf[3] << 8) | buf[2];
//     calib.dig_t3 = (int16_t)(buf[5] << 8) | buf[4];

//     /* Parse pressure calibration */
//     calib.dig_p1 = (uint16_t)(buf[7] << 8) | buf[6];
//     calib.dig_p2 = (int16_t)(buf[9] << 8) | buf[8];
//     calib.dig_p3 = (int16_t)(buf[11] << 8) | buf[10];
//     calib.dig_p4 = (int16_t)(buf[13] << 8) | buf[12];
//     calib.dig_p5 = (int16_t)(buf[15] << 8) | buf[14];
//     calib.dig_p6 = (int16_t)(buf[17] << 8) | buf[16];
//     calib.dig_p7 = (int16_t)(buf[19] << 8) | buf[18];
//     calib.dig_p8 = (int16_t)(buf[21] << 8) | buf[20];
//     calib.dig_p9 = (int16_t)(buf[23] << 8) | buf[22];

//     /* Read humidity calibration part 1 (0xA1) */
//     ret = i2c_burst_read_dt(dev, 0xA1, buf, 1);
//     if (ret != 0) return ret;
//     calib.dig_h1 = buf[0];

//     /* Read humidity calibration part 2 (0xE1 - 0xE7) */
//     ret = i2c_burst_read_dt(dev, REG_CALIB26, buf, 7);
//     if (ret != 0) return ret;

//     calib.dig_h2 = (int16_t)(buf[1] << 8) | buf[0];
//     calib.dig_h3 = buf[2];
//     calib.dig_h4 = (int16_t)(buf[3] << 4) | (buf[4] & 0x0F);
//     calib.dig_h5 = (int16_t)(buf[5] << 4) | (buf[4] >> 4);
//     calib.dig_h6 = (int8_t)buf[6];

//     printk("Calibration data loaded successfully\n");
//     return 0;
// }

// /* Compensate temperature (returns temperature in 0.01 degrees C) */
// int32_t bme280_compensate_temp(int32_t adc_temp)
// {
//     int32_t var1, var2, temperature;

//     var1 = ((((adc_temp >> 3) - ((int32_t)calib.dig_t1 << 1))) * 
//             ((int32_t)calib.dig_t2)) >> 11;

//     var2 = (((((adc_temp >> 4) - ((int32_t)calib.dig_t1)) * 
//               ((adc_temp >> 4) - ((int32_t)calib.dig_t1))) >> 12) * 
//             ((int32_t)calib.dig_t3)) >> 14;

//     calib.t_fine = var1 + var2;
//     temperature = (calib.t_fine * 5 + 128) >> 8;

//     return temperature;
// }

// /* Compensate pressure (returns pressure in Pa as unsigned 32-bit integer) */
// uint32_t bme280_compensate_press(int32_t adc_press)
// {
//     int64_t var1, var2, pressure;

//     var1 = ((int64_t)calib.t_fine) - 128000;
//     var2 = var1 * var1 * (int64_t)calib.dig_p6;
//     var2 = var2 + ((var1 * (int64_t)calib.dig_p5) << 17);
//     var2 = var2 + (((int64_t)calib.dig_p4) << 35);
//     var1 = ((var1 * var1 * (int64_t)calib.dig_p3) >> 8) + 
//            ((var1 * (int64_t)calib.dig_p2) << 12);
//     var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calib.dig_p1) >> 33;

//     if (var1 == 0) {
//         return 0;  /* Avoid division by zero */
//     }

//     pressure = 1048576 - adc_press;
//     pressure = (((pressure << 31) - var2) * 3125) / var1;
//     var1 = (((int64_t)calib.dig_p9) * (pressure >> 13) * (pressure >> 13)) >> 25;
//     var2 = (((int64_t)calib.dig_p8) * pressure) >> 19;
//     pressure = ((pressure + var1 + var2) >> 8) + (((int64_t)calib.dig_p7) << 4);

//     return (uint32_t)(pressure >> 8);
// }

// /* Compensate humidity (returns humidity in %RH as unsigned 32-bit integer in Q22.10 format) */
// uint32_t bme280_compensate_humidity(int32_t adc_hum)
// {
//     int32_t var1;

//     var1 = calib.t_fine - ((int32_t)76800);
//     var1 = ((((adc_hum << 14) - (((int32_t)calib.dig_h4) << 20) - 
//               (((int32_t)calib.dig_h5) * var1)) + ((int32_t)16384)) >> 15) * 
//            (((((((var1 * ((int32_t)calib.dig_h6)) >> 10) * 
//                 (((var1 * ((int32_t)calib.dig_h3)) >> 11) + ((int32_t)32768))) >> 10) + 
//               ((int32_t)2097152)) * ((int32_t)calib.dig_h2) + 8192) >> 14);

//     var1 = var1 - (((((var1 >> 15) * (var1 >> 15)) >> 7) * 
//                     ((int32_t)calib.dig_h1)) >> 4);

//     var1 = (var1 < 0) ? 0 : var1;
//     var1 = (var1 > 419430400) ? 419430400 : var1;

//     return (uint32_t)(var1 >> 12);
// }

// void startBMESensor(void)
// {
//     static const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(I2C_NODE);
//     int ret;
//     uint8_t chip_id;

//     printk("\n\n");
//     printk("==========================================\n");
//     printk("  BME280 - Temperature/Pressure/Humidity\n");
//     printk("==========================================\n\n");

//     /* Check I2C bus */
//     if (!device_is_ready(dev_i2c.bus)) {
//         printk("I2C bus %s is not ready!\n", dev_i2c.bus->name);
//     }
//     printk("I2C bus ready: %s\n", dev_i2c.bus->name);
//     printk("I2C address: 0x%02X\n\n", dev_i2c.addr);

//     /* Read and verify chip ID */
//     uint8_t reg = REG_ID;
//     ret = i2c_write_read_dt(&dev_i2c, &reg, 1, &chip_id, 1);
// 	printk("Return error %x \n", ret);
//     if (ret != 0) {
//         printk("Failed to read chip ID!\n");
//     }

//     if (chip_id == BME280_CHIP_ID) {
//         printk("Detected: BME280 (ID: 0x%02X)\n", chip_id);
//     } else if (chip_id == BMP280_CHIP_ID) {
//         printk("Detected: BMP280 (ID: 0x%02X) - No humidity!\n", chip_id);
//     } else {
//         printk("Unknown chip ID: 0x%02X\n", chip_id);
//     }
//     sensor_channel_get(bme, SENSOR_CHAN_AMBIENT_TEMP, &temp);

//     /* Read calibration data */
//     ret = bme280_read_calibration(&dev_i2c);
//     if (ret != 0) {
//         printk("Failed to read calibration!\n");
//     }

//     /* Configure sensor */
//     /* Set humidity oversampling (must be set before ctrl_meas) */
//     uint8_t ctrl_hum[] = {REG_CTRL_HUM, 0x01};  /* Humidity oversampling x1 */
//     ret = i2c_write_dt(&dev_i2c, ctrl_hum, 2);
//     if (ret != 0) {
//         printk("Failed to configure humidity!\n");
//     }

//     /* Set temperature/pressure oversampling and mode */
//     /* 0x27 = temp x1, press x1, normal mode */
//     uint8_t ctrl_meas[] = {REG_CTRL_MEAS, 0x27};
//     ret = i2c_write_dt(&dev_i2c, ctrl_meas, 2);
//     if (ret != 0) {
//         printk("Failed to configure sensor!\n");
//     }

//     printk("Sensor configured successfully!\n\n");

//     /* Main loop - read all sensor data */
//     while (1) {
//         uint8_t data[8];  /* Press(3) + Temp(3) + Hum(2) = 8 bytes */

//         /* Read all data registers (0xF7 to 0xFE) */
//         ret = i2c_burst_read_dt(&dev_i2c, REG_PRESS_MSB, data, 8);
//         if (ret != 0) {
//             printk("Failed to read sensor data!\n");
//             k_msleep(SLEEP_TIME_MS);
//             continue;
//         }

//         /* Parse raw ADC values */
//         int32_t adc_press = (int32_t)((data[0] << 12) | (data[1] << 4) | (data[2] >> 4));
//         int32_t adc_temp  = (int32_t)((data[3] << 12) | (data[4] << 4) | (data[5] >> 4));
//         int32_t adc_hum   = (int32_t)((data[6] << 8) | data[7]);

//         /* Compensate values */
//         int32_t temp_raw = bme280_compensate_temp(adc_temp);      /* 0.01 °C */
//         uint32_t press_raw = bme280_compensate_press(adc_press);  /* Pa */
//         uint32_t hum_raw = bme280_compensate_humidity(adc_hum);   /* Q22.10 format */

//         /* Convert to readable units */
//         float temperature = temp_raw / 100.0f;           /* °C */
//         float pressure = press_raw / 1000.0f;            /* kPa */
//         float humidity = hum_raw / 1024.0f;              /* %RH */

//         /* Print results */
//         printk("------------------------------------------\n");
//         printk("Temperature: %.2f C (%.2f F)\n", 
//                (double)temperature, (double)(temperature * 1.8f + 32.0f));
//         printk("Pressure:    %.2f kPa (%.2f hPa)\n", 
//                (double)pressure, (double)(pressure * 10.0f));
//         printk("Humidity:    %.2f %%RH\n", (double)humidity);

//         /* Heat stress warning */
//         if (temperature > 35.0f) {
//             printk("*** WARNING: High temperature! ***\n");
//         }
//         if (humidity > 80.0f) {
//             printk("*** WARNING: High humidity! ***\n");
//         }

//         k_msleep(SLEEP_TIME_MS);
//     }

// }