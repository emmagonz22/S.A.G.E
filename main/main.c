#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "web_server.h"
#include "sensor_processing.h"
#include "command_types.h"
#include "freertos/queue.h"
#include "esp_mac.h" 

/* Access Point Configuration, this data may change */
#define ESP_WIFI_AP_SSID      "SAGE-ESP32-AP"      // Your AP name
#define ESP_WIFI_AP_PASS      "sageNetwork@123"   // Your AP password (min 8 chars)
#define ESP_WIFI_CHANNEL      1               // WiFi channel
#define MAX_STA_CONN          4               // Maximum connections allowed


static const char *TAG = "main";
QueueHandle_t command_queue;

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
char name_device[64] = {0};  // Safe fixed-size buffer
void set_name_device(char *name) {
    if (name != NULL) {
        strncpy(name_device, name, 64 - 1);
        name_device[64 - 1] = '\0';  // Null-terminate just in case
        ESP_LOGI("Naming Module", "Device name set to: %s", name_device);
    } else {
        ESP_LOGW("Naming Module", "Received NULL name!");
        name_device[0] = '\0';
    }
}


// All commands to the Internal Tool or the app shuld be done here.
void main_command_handler(void *arg) {
    CommandMessage msg;

    while (1) {
        if (xQueueReceive(command_queue, &msg, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Command received: %d", msg.type);
            switch (msg.type) {
                case CMD_START_SENSOR:
                    start_sensor();  // From sensor_processing.c
                    break;
                case CMD_STOP_SENSOR:
                    ESP_LOGI(TAG, "CMD_STOP_SENSOR received. name_device: %s", name_device);
                    stop_sensor(name_device);   // From sensor_processing.c
                    break;
                case CMD_SET_NAME:
                    ESP_LOGI(TAG, "CMD_SET_NAME received. Payload: %s", msg.payload);
                    set_name_device(msg.payload); // Set global or static name_device
                    ESP_LOGI(TAG, "Device name set to: %s", name_device);
                    break;
                case CMD_TOGGLE_SENSOR:
                    toggle_sensor();
                    break;
                default:
                    break;
            }
        }
    }
}


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
    //sensor_init();

    command_queue = xQueueCreate(10, sizeof(CommandMessage));

    xTaskCreate(main_command_handler, "CommandHandler", 4096, NULL, 5, NULL);

    // Pass the queue to web server
    ESP_ERROR_CHECK(start_web_server(command_queue));
    ESP_LOGI(TAG, "Web server started");

    esp_netif_ip_info_t ip_info;
    // LOG the IP address of the server into the console
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP"), &ip_info);
    ESP_LOGI("WIFI", "AP IP Address: " IPSTR, IP2STR(&ip_info.ip));

    // Later: pass to app interface
    write_test_csv_to_flash();
}

