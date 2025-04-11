#include "sensor_processing.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
//#include <Arduino.h>
#include "esp_timer.h"
#include "dht/dht.h"
#include "dht.h"
#include "esp_log.h"

#include "ds18x20.h"
#include "ds18x20/ds18x20.h"
#include "onewire.h"
#include "onewire/onewire.h"
#include "esp_spiffs.h"
#include <driver/adc.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// Tasks
static TaskHandle_t sensor_task_handle = NULL;

// Initialize sensor instances
// DHT dht(DHTPIN, DHTTYPE);
// OneWire oneWire(ONE_WIRE_BUS);
// DallasTemperature ds18b20(&oneWire);

// Timer variables
unsigned long prevMillis = 0;
const long interval = 1000; // 1-second interval for reading data
int64_t millis() {
    return esp_timer_get_time() / 1000;
}

// Store Sensor Values
float dht_humidity, dht_temperature, ds18_temperature;

void sensor_init() {
    // ds18b20.begin();
    // pinMode(MOISTURE_PIN, INPUT);

    // // Print CSV header (this will be printed once at startup)
    // Serial.println("Timestamp (ms), Soil Moisture (%), Air Humidity (%), Soil Temp (°C), Air Temp (°C)");
    adc1_config_width(ADC_WIDTH_BIT_12);  // 12-bit resolution (0 - 4095)
    adc1_config_channel_atten(ANALOG_SENSOR_PIN_MOISTURE, ADC_ATTEN_DB_11); // Full voltage range (0-3.3V)
    init_spiffs();
}

SensorData read_sensors() {
    SensorData data;
    int raw_value;

    unsigned long currMillis = millis();
;
    if (currMillis - prevMillis >= interval) {
        prevMillis = currMillis; // Reset timer

    // Read DHT11 sensor
        esp_err_t result = dht_read_float_data(DHT_TYPE_DHT11, DHTPIN, &dht_humidity, &dht_temperature);
        data.temperatureDHT = dht_temperature;
        data.humidity = dht_humidity;
        
    // Read DS18B20 sensor
        // We need to use the DS18B20 instead of ds18x20
        // since the sensor addr. doens't seem to match
        // the manufacturer correctly.

        result = ds18b20_read_temperature(ONE_WIRE_BUS, DS18X20_ANY, &ds18_temperature);
        data.temperatureDS18B20 = ds18_temperature;
    // Read Soil Moisture Sensor (analog)

        // This one is independent of Arduino Libraries
        raw_value = adc1_get_raw(ANALOG_SENSOR_PIN_MOISTURE);

        // analogRead comes from Arduino libraries
        // The reason for not using it, is seeing if maybe we can remove the Arduino dependency to avoid the RTOS error
        //raw_value = analogRead(MOISTURE_PIN);
        data.moisturePercent = raw_value;
        
        // Convert raw value to percentage
        data.moisturePercent = map_value(raw_value, AIR_VALUE, WATER_VALUE, 0.0, 100.0);
        data.moisturePercent = constrain_value(data.moisturePercent, 0.0, 100.0); // Keep within range

        data.currentMillis = currMillis;

        // Print data in CSV format
        // Serial.print(currMillis); Serial.print(", ");
        // Serial.print(data.moisturePercent); Serial.print(", ");
        // Serial.print(data.humidity); Serial.print(", ");
        // Serial.print(data.temperatureDS18B20); Serial.print(", ");
        // Serial.println(data.temperatureDHT);
    }
    ESP_LOGI("SENSOR_MODULE", "Timestamp: %lu ms | Moisture: %.2f%% | Humidity: %.2f%% | Soil Temp: %.2f°C | Air Temp: %.2f°C", 
                 data.currentMillis, data.moisturePercent, data.humidity, data.temperatureDS18B20, data.temperatureDHT);
    append_csv_row(data.currentMillis, data.moisturePercent, data.humidity, data.temperatureDS18B20, data.temperatureDHT);
    //read_csv_from_flash();
    return data;
}

// Helper Functions
// Some functions are only in Arduino C
// These are the C equivalent Functions
float map_value(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float constrain_value(float value, float min_val, float max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}


// Storage: Saving the Sensor data into a CSV
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
        ESP_LOGE(TAG1, "CSV buffer is almost full!");
    }
}

void save_csv_to_flash() {
    FILE *f = fopen("/csv_logs/session_log.csv", "w");
    if (f == NULL) {
        ESP_LOGE(TAG1, "Failed to open file for writing");
        return;
    }
    fprintf(f, "Timestamp,Moisture,Humidity,SoilTemp,AirTemp\n"); // Header
    fwrite(csv_buffer, 1, buffer_index, f);
    fclose(f);
    ESP_LOGI(TAG1, "CSV file saved to /storage/session_log.csv");
}


/* Initialize SPIFFS */
static esp_err_t init_spiffs(void)
{
    ESP_LOGI(TAG1, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/csv_logs",
        .partition_label = "csv_logs",
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG1, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG1, "Failed to find CSV partition");
        } else {
            ESP_LOGE(TAG1, "Failed to initialize CSV SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info("csv_logs", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG1, "Failed to get CSV SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG1, "Partition size: total: %d, used: %d", total, used);
    }
    
    return ESP_OK;
}

// CSV Portion 
void read_csv_from_flash() {
    // if (!esp_spiffs_mounted("/csv_logs")) {
    //     ESP_LOGE(TAG1, "SPIFFS is not mounted, cannot read CSV");
    //     return;
    // }

    FILE *f = fopen("/csv_logs/session_log.csv", "r");
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

// Define maint ask functions: To be used by main.c

void sensor_task(void *pvParameters) {
    while (1) {
        SensorData data = read_sensors();
        ESP_LOGI(TAG1, "read_sensors was executed.");
        if (stopLogging) {
            vTaskSuspend(NULL);  // Suspend this task until resumed
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay 1 second
    }
}

void start_sensor() {
    if (sensor_task_handle == NULL) {
        xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, &sensor_task_handle);
        ESP_LOGI(TAG1, "Sensor task started.");
    } else {
        vTaskResume(sensor_task_handle);
        ESP_LOGI(TAG1, "Sensor task resumed.");
    }
}

void stop_sensor() {
    if (sensor_task_handle != NULL) {
        vTaskSuspend(sensor_task_handle);
        ESP_LOGI(TAG1, "Sensor task suspended.");
        save_csv_to_flash();
        read_csv_from_flash();
    }
}