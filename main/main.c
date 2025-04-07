#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "web_server.h"
#include "esp_mac.h" 
#include "sensor_processing.h"
#include "esp_spiffs.h"

/* Access Point Configuration, this data may change */
#define ESP_WIFI_AP_SSID      "SAGE-ESP32-AP"      // Your AP name
#define ESP_WIFI_AP_PASS      "sageNetwork@123"   // Your AP password (min 8 chars)
#define ESP_WIFI_CHANNEL      1               // WiFi channel
#define MAX_STA_CONN          4               // Maximum connections allowed


static const char *TAG = "main";

// https://esp32tutorials.com/esp32-access-point-ap-esp-idf/

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
        MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
        MAC2STR(event->mac), event->aid);
    }
}


void wifi_init_softap(void)
{
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ESP_WIFI_AP_SSID,
            .ssid_len = strlen(ESP_WIFI_AP_SSID),
            .password = ESP_WIFI_AP_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    if (strlen(ESP_WIFI_AP_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    if (strlen(ESP_WIFI_AP_PASS) < 8) {
        ESP_LOGE(TAG, "AP password must be at least 8 characters");
        return;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// Sensor Reading Collection
void sensor_task(void *pvParameter){
    while(1){
        SensorData sensorValues =read_sensors();
        ESP_LOGI("SENSOR_MODULE", "Timestamp: %lu ms | Moisture: %.2f%% | Humidity: %.2f%% | Soil Temp: %.2f°C | Air Temp: %.2f°C", 
                 sensorValues.currentMillis, sensorValues.moisturePercent, sensorValues.humidity, sensorValues.temperatureDS18B20, sensorValues.temperatureDHT);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static const char *TAG1 = "SENSOR_MODULE";
bool stopLogging = false;
char csv_buffer[4096];  // ~4KB buffer for storing session
size_t buffer_index = 0;

// CSV Portion - Probaly should be moved to the sensor processing
void append_csv_row(uint32_t timestamp, float moisture, float humidity, float soil_temp, float air_temp) {
    // Append to buffer
    buffer_index += snprintf(&csv_buffer[buffer_index],
        sizeof(csv_buffer) - buffer_index,
        "%lu,%.2f,%.2f,%.2f,%.2f\n",
        timestamp, moisture, humidity, soil_temp, air_temp);

    // Avoid buffer overflow
    if (buffer_index >= sizeof(csv_buffer) - 100) {
        ESP_LOGW(TAG1, "CSV buffer is almost full!");
    }
}

void save_csv_to_flash() {
    FILE *f = fopen("/storage/session_log.csv", "w");
    if (f == NULL) {
        ESP_LOGE(TAG1, "Failed to open file for writing");
        return;
    }
    fprintf(f, "Timestamp,Moisture,Humidity,SoilTemp,AirTemp\n"); // Header
    fwrite(csv_buffer, 1, buffer_index, f);
    fclose(f);
    ESP_LOGI(TAG1, "CSV file saved to /storage/session_log.csv");
}


void init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/storage",
      .partition_label = "storage",
      .max_files = 5,
      .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG1, "Failed to mount or format filesystem");
    } else {
        ESP_LOGI(TAG1, "SPIFFS mounted successfully");
    }
}

void read_csv_from_flash() {
    FILE *f = fopen("/storage/session_log.csv", "r");
    if (f == NULL) {
        ESP_LOGE(TAG1, "Failed to open CSV file for reading");
        return;
    }

    char line[128]; // Adjust buffer size as needed
    while (fgets(line, sizeof(line), f)) {
        ESP_LOGI(TAG1, "CSV: %s", line);  // Output to serial console
    }

    fclose(f);
}


//---------------------------------------------

void app_main(void)
{
    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Connect to WiFi */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "Setting up WiFi Access Point...");
    wifi_init_softap();

    /* Start web server */
    ESP_ERROR_CHECK(start_web_server());
    ESP_LOGI(TAG, "Web server started");

    esp_netif_ip_info_t ip_info;
    // LOG the IP address of the server into the console
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP"), &ip_info);
    ESP_LOGI("WIFI", "AP IP Address: " IPSTR, IP2STR(&ip_info.ip));

    sensor_init();
    SensorData sensorValues = read_sensors();


    ESP_LOGI("SENSOR_MODULE", "Timestamp: %lu ms | Moisture: %.2f%% | Humidity: %.2f%% | Soil Temp: %.2f°C | Air Temp: %.2f°C", 
                 sensorValues.currentMillis, sensorValues.moisturePercent, sensorValues.humidity, sensorValues.temperatureDS18B20, sensorValues.temperatureDHT);

    xTaskCreate(&sensor_task, "Sensor Task", 4096, NULL, 5, NULL);

    // CSV Creation
    init_spiffs();
    append_csv_row(sensorValues.currentMillis, sensorValues.moisturePercent, sensorValues.humidity, sensorValues.temperatureDS18B20, sensorValues.temperatureDHT);
    save_csv_to_flash();
    read_csv_from_flash();
}