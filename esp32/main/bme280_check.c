#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "globals.h"
#include "i2c_reader.h"

void app_main(void)
{
    sensor_power_on();

    esp_err_t err = i2c_reader_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BME280 init failed: %s", esp_err_to_name(err));
        return;
    }

    while (1) {
        float temp_c, hum_pct, press_hpa;
        err = i2c_reader_read(&temp_c, &hum_pct, &press_hpa);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "T=%.2f C  H=%.2f %%  P=%.2f hPa", temp_c, hum_pct, press_hpa);
        } else {
            ESP_LOGE(TAG, "Read failed: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
