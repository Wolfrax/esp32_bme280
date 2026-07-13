#ifndef I2C_READER_H
#define I2C_READER_H

#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SENSOR_PWR GPIO_NUM_5

void sensor_power_on(void);
void sensor_power_off(void);
esp_err_t i2c_reader_init(void);
esp_err_t i2c_reader_read(float *temp_c, float *hum_pct, float *press_hpa);
i2c_master_bus_handle_t i2c_reader_get_bus_handle(void);

#ifdef __cplusplus
}
#endif

#endif // I2C_READER_H