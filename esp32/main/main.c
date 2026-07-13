#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "globals.h"
#include "i2c_reader.h"
#include "max17048.h"
#include "wifi.h"
#include "esp_wifi.h"
#include "esp_crt_bundle.h"

#define DEV_MODE 0 // If set to 1, the device will not go to deep sleep and will print logs to the console for debugging.
#define SIMULATE_SENSOR 0 // If set to 1, the device will simulate sensor readings instead of reading from the actual sensor, for testing without hardware.

#if SIMULATE_SENSOR
#include "esp_random.h"
#endif

#define SLEEP_INTERVAL (10*60*1000*1000) // 10 minutes = 10 * 60 * 1000 * 1000 microseconds

RTC_DATA_ATTR static char starttime_str[64] = "UNKNOWN";
RTC_DATA_ATTR static int boot_count = 0;

#define LOG_LINES      16
#define LOG_LINE_LEN   128

RTC_DATA_ATTR static char log_buffer[LOG_LINES][LOG_LINE_LEN];
RTC_DATA_ATTR static int log_index = 0;

typedef struct {
    char ts[32];
    float t, h, p;
    float batt_v, batt_pct;
    bool batt_ok;
} sample_t;

static sample_t latest_sample = {0};

static void utc_timestamp_now(char *buf, size_t len)
{
    time_t now;
    time(&now);

    struct tm tinfo;
    gmtime_r(&now, &tinfo);

    strftime(
        buf,
        len,
        "%Y-%m-%dT%H:%M:%SZ",
        &tinfo
    );
}

void add_log(const char *fmt, ...)
{
    char msg[96];
    char ts[32];

    utc_timestamp_now(ts, sizeof(ts));

    va_list args;
    va_start(args, fmt);

    vsnprintf(msg, sizeof(msg), fmt, args);

    va_end(args);

    snprintf(
        log_buffer[log_index],
        LOG_LINE_LEN,
        "%s %s",
        ts,
        msg
    );

    log_index = (log_index + 1) % LOG_LINES;
}

static void go_to_deep_sleep(void)
{
    add_log("Stopping WiFi before deep sleep");

    esp_wifi_disconnect();
    esp_wifi_stop();

    esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL);
    add_log("Entering deep sleep for %" PRIu64 " us", (uint64_t)SLEEP_INTERVAL);
    
    esp_deep_sleep_start();
}

char *make_post_json(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *sample = cJSON_CreateObject();
    cJSON *node = cJSON_CreateObject();

    cJSON_AddStringToObject(sample, "ts", latest_sample.ts);
    cJSON_AddNumberToObject(sample, "t", latest_sample.t);
    cJSON_AddNumberToObject(sample, "h", latest_sample.h);
    cJSON_AddNumberToObject(sample, "p", latest_sample.p);

    if (latest_sample.batt_ok) {
        cJSON_AddNumberToObject(sample, "batt_v", latest_sample.batt_v);
        cJSON_AddNumberToObject(sample, "batt_pct", latest_sample.batt_pct);
    }

    cJSON_AddItemToObject(root, "sample", sample);

    cJSON_AddStringToObject(node, "name", hostname);
    cJSON_AddStringToObject(node, "ip", ip_str);
    cJSON_AddStringToObject(node, "ssid", current_ssid);
    cJSON_AddStringToObject(node, "starttime", starttime_str);
    cJSON_AddStringToObject(node, "mac", macstr);
    cJSON_AddNumberToObject(node, "boot_count", boot_count);
    cJSON_AddStringToObject(node, "location", LOCATION_STR);
    cJSON_AddNumberToObject(node, "sample_interval_us", SLEEP_INTERVAL);
    cJSON_AddItemToObject(root, "node", node);

    cJSON *logs = cJSON_CreateArray();

    for (int i = 0; i < LOG_LINES; i++) {
        int idx = (log_index + i) % LOG_LINES;

        if (strlen(log_buffer[idx]) > 0) {
            cJSON_AddItemToArray(
                logs,
                cJSON_CreateString(log_buffer[idx])
            );
        }
    }

    cJSON_AddItemToObject(root, "logs", logs);

    char *json = cJSON_PrintUnformatted(root);

    cJSON_Delete(root);

    return json;
}

static void send_to_server(void)
{   
    char *post_data = make_post_json();

    if (post_data != NULL) {
        esp_http_client_config_t config = {
            .url = SERVER_URL,
            .method = HTTP_METHOD_POST,
            .timeout_ms = 10000,
            .keep_alive_enable = false,
            .crt_bundle_attach = esp_crt_bundle_attach,
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (client == NULL) {
            add_log("Failed to initialize HTTP client");
            return;
        }   

        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, post_data, strlen(post_data));
        esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK) {
            add_log("HTTP POST Status = %d, content_length = %" PRId64,
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
        } else {
            add_log("HTTP POST request failed: %s", esp_err_to_name(err));
        }

        esp_http_client_cleanup(client);
        free(post_data);
    }
    else {
        add_log("Failed to create JSON payload");
    }
}

#if SIMULATE_SENSOR
static float get_temperature() { return 20 + (esp_random() % 100) / 10.0; }
static float get_humidity()    { return 40 + (esp_random() % 200) / 10.0; }
static float get_pressure()    { return 1000 + (esp_random() % 100); }
#endif

static void sample_and_post(void)
{
    esp_err_t err;
    sample_t s;

    time_t now;
    time(&now);

    struct tm tinfo;
    gmtime_r(&now, &tinfo);

    strftime(s.ts, sizeof(s.ts), "%Y-%m-%dT%H:%M:%SZ", &tinfo);

#if SIMULATE_SENSOR
    s.t = get_temperature();
    s.h = get_humidity();
    s.p = get_pressure();
    err = ESP_OK;
    s.batt_ok = false;
#else
    err = i2c_reader_read(&s.t, &s.h, &s.p);
    if (err != ESP_OK) {
        add_log("Read failed: %s", esp_err_to_name(err));
    }

    s.batt_ok = (max17048_read(&s.batt_v, &s.batt_pct) == ESP_OK);
    if (s.batt_ok) {
        add_log("Battery: %.1f%% %.2fV", s.batt_pct, s.batt_v);
    }
#endif

    if (err == ESP_OK) {
        latest_sample = s;
        send_to_server();
    }

}

/* ---------------- TIME ---------------- */

static void obtain_time(void)
{
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    // Set timezone (Sweden)
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();

    time_t now = 0;
    struct tm timeinfo = { 0 };

    while (timeinfo.tm_year < (2020 - 1900)) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (strcmp(starttime_str, "UNKNOWN") == 0) {
        // Only set starttime_str on the first boot after flashing, not on every deep sleep wakeup
        strftime(starttime_str, sizeof(starttime_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
    }
    
}

/* ---------------- MAIN ---------------- */

void app_main(void)
{
    boot_count++;

    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init_base();
    wifi_start_sta();

    esp_err_t wifi_err = wifi_wait_connected(10000);
    if (wifi_err != ESP_OK) {
        add_log("WiFi/IP timeout");
        go_to_deep_sleep();
    }

    if ((boot_count % 24) == 1) {
        // Only obtain time from NTP every 24 boots to save time and power, since we only need an approximate time for the timestamp.
        // For 1 minute sampling interval, use: if ((boot_count % 60) == 1). 
        obtain_time();
        add_log("System time obtained: %s", starttime_str);
    }
  
    add_log("%s ready (boot: %d)", hostname, boot_count);

#if SIMULATE_SENSOR
    sample_and_post();
#else
    sensor_power_on();

    esp_err_t err = i2c_reader_init();
    if (err != ESP_OK) {
        add_log("Sensor init failed: %s", esp_err_to_name(err));
        sensor_power_off();
        vTaskDelay(pdMS_TO_TICKS(500));
        go_to_deep_sleep();
    }

    err = max17048_init();
    if (err != ESP_OK) {
        add_log("Battery gauge not available: %s", esp_err_to_name(err));
    }

    sample_and_post();
    sensor_power_off();
#endif
    vTaskDelay(pdMS_TO_TICKS(500));

#if DEV_MODE
    ESP_LOGI(TAG, "DEV_MODE: no sleep");
#else
    go_to_deep_sleep();
#endif

}
