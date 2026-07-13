#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_err.h"
#include "max17048.h"
#include "i2c_reader.h"
#include "globals.h"

#define MAX17048_ADDR   0x36   // fixed in silicon, not configurable
#define MAX17048_FREQ_HZ 100000

#define REG_VCELL       0x02
#define REG_SOC         0x04

static i2c_master_dev_handle_t dev_handle = NULL;

esp_err_t max17048_init(void)
{
    i2c_master_bus_handle_t bus_handle = i2c_reader_get_bus_handle();
    if (bus_handle == NULL) {
        ESP_LOGE(TAG, "MAX17048: I2C bus not initialized (call i2c_reader_init() first)");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = i2c_master_probe(bus_handle, MAX17048_ADDR, 100);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MAX17048 not found at 0x%02X: %s", MAX17048_ADDR, esp_err_to_name(err));
        return ESP_ERR_NOT_FOUND;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MAX17048_ADDR,
        .scl_speed_hz = MAX17048_FREQ_HZ,
    };
    err = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed adding MAX17048 device: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "MAX17048 ready at 0x%02X", MAX17048_ADDR);
    return ESP_OK;
}

esp_err_t max17048_read(float *volts, float *soc_pct)
{
    if (volts == NULL || soc_pct == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (dev_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buf[2];

    uint8_t reg = REG_VCELL;
    esp_err_t err = i2c_master_transmit_receive(dev_handle, &reg, 1, buf, sizeof(buf), 100);
    if (err != ESP_OK) {
        return err;
    }
    uint16_t raw_vcell = ((uint16_t)buf[0] << 8) | buf[1];
    *volts = raw_vcell * 78.125e-6f;

    reg = REG_SOC;
    err = i2c_master_transmit_receive(dev_handle, &reg, 1, buf, sizeof(buf), 100);
    if (err != ESP_OK) {
        return err;
    }
    uint16_t raw_soc = ((uint16_t)buf[0] << 8) | buf[1];
    *soc_pct = (raw_soc >> 8) + (float)(raw_soc & 0xFF) / 256.0f;

    return ESP_OK;
}
