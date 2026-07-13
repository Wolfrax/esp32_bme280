#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"

#define TAG           "I2C_SCAN"
#define MAX17048_ADDR 0x36   // fixed in silicon, not configurable

void app_main(void)
{
    gpio_config_t pwr = { .pin_bit_mask = (1ULL << 5), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&pwr);
    gpio_set_level(5, 1);
    vTaskDelay(pdMS_TO_TICKS(300));

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port          = I2C_NUM_0,
        .sda_io_num        = 6,
        .scl_io_num        = 7,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    ESP_LOGI(TAG, "Full scan 0x08-0x77 at 100kHz (expecting MAX17048 at 0x%02X)", MAX17048_ADDR);

    int found = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        esp_err_t err = i2c_master_probe(bus, addr, 20);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "  *** Found device at 0x%02X ***", addr);
            found++;
        } else if (err != ESP_ERR_NOT_FOUND) {
            ESP_LOGW(TAG, "  0x%02X unexpected: %s", addr, esp_err_to_name(err));
        }
    }

    if (found == 0) {
        ESP_LOGW(TAG, "No devices found.");
        return;
    }

    ESP_LOGI(TAG, "Total: %d device(s). Reading MAX17048 registers at 0x%02X...", found, MAX17048_ADDR);

    i2c_device_config_t dc = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = MAX17048_ADDR,
        .scl_speed_hz    = 100000,
    };
    i2c_master_dev_handle_t dev;
    if (i2c_master_bus_add_device(bus, &dc, &dev) != ESP_OK) return;

    const struct { uint8_t reg; const char *name; } regs[] = {
        {0x08, "VERSION"}, {0x02, "VCELL  "}, {0x04, "SOC    "},
    };
    for (int iter = 0; iter < 5; iter++) {
        ESP_LOGI(TAG, "--- read %d/5 ---", iter + 1);
        for (int i = 0; i < 3; i++) {
            uint8_t buf[2] = {0};
            esp_err_t e = i2c_master_transmit_receive(dev, &regs[i].reg, 1, buf, 2, 100);
            if (e != ESP_OK) {
                ESP_LOGW(TAG, "  %s read failed: %s", regs[i].name, esp_err_to_name(e));
                continue;
            }
            uint16_t val = ((uint16_t)buf[0] << 8) | buf[1];
            if (i == 0)
                ESP_LOGI(TAG, "  %s = 0x%04X", regs[i].name, val);
            else if (i == 1)
                ESP_LOGI(TAG, "  %s = 0x%04X → %.3f V", regs[i].name, val, val * 0.000078125f);
            else
                ESP_LOGI(TAG, "  %s = 0x%04X → %.1f%%", regs[i].name, val,
                         (val >> 8) + (float)(val & 0xFF) / 256.0f);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    i2c_master_bus_rm_device(dev);
}
