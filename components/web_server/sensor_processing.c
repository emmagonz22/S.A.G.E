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
#include "driver/uart.h"
#include "driver/gpio.h"
#include <math.h>

// Tasks
static TaskHandle_t sensor_task_handle = NULL;

// Initialize sensor instances
// DHT dht(DHTPIN, DHTTYPE);
// OneWire oneWire(ONE_WIRE_BUS);
// DallasTemperature ds18b20(&oneWire);

// Timer variables
unsigned long prevMillis = 0;
const long interval = 1000; // 1-second interval for reading data
static int64_t sensor_start_time = 0;
int64_t millis() {
    return (esp_timer_get_time() - sensor_start_time) / 1000;
}

// Needed a new way to reset time without depending on the ESPs timer
// int64_t millis() {
//     return esp_timer_get_time() / 1000;
// }

// Store Sensor Values
float dht_humidity, dht_temperature, ds18_temperature, rs485_ph, rs485_n, rs485_p, rs485_k, rs485_ec, rs485_moisture;

void sensor_init() {

    onewire_reset(ONE_WIRE_BUS);
    adc1_config_width(ADC_WIDTH_BIT_12);  // 12-bit resolution (0 - 4095)
    adc1_config_channel_atten(ANALOG_SENSOR_PIN_MOISTURE, ADC_ATTEN_DB_11); // Full voltage range (0-3.3V)
    
    // GPIO setup
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RE_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(RE_PIN, 0);

    // UART config
    uart_config_t uart_config = {
        .baud_rate = 4800,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_PORT, 256, 0, 0, NULL, 0);
}

// NPK Sensor Functions
// Need to create my own library to access the values
void send_command(const uint8_t* command, size_t length) {
    gpio_set_level(RE_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    uart_write_bytes(UART_PORT, (const char*)command, length);
    uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(100));
    gpio_set_level(RE_PIN, 0);
}
// Reads a sensor and outputs the result through a float pointer
esp_err_t read_value_485(const char *label, const uint8_t *command, bool divideBy10, const char *unit, float *out_value) {
    if (out_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t response[8] = {0};
    send_command(command, 8);

    // Wait for response
    int len = uart_read_bytes(UART_PORT, response, sizeof(response), pdMS_TO_TICKS(200));
    if (len >= 5) {
        int raw = (response[3] << 8) | response[4];
        float value = divideBy10 ? raw / 10.0f : raw;
        *out_value = value;

        printf("%s: %.1f %s\n", label, value, unit);
        return ESP_OK;
    } else {
        ESP_LOGW(TAG1, "%s: Read error (got %d bytes)", label, len);
        return ESP_FAIL;
    }
}

// Estimations of Soil Nutrients
// Based on agronomic patterns and real-world studies:
// USDA NRCS Soil Nutrient Guidelines, FAO Soil Fertility Handbook (FAO, 2006),
// "Soil Fertility and Fertilizers" by Havlin et al. 
// Yadav et al. (2012) – Correlation between EC and macronutrients in Indian soils.
// https://www.mdpi.com/1424-8220/20/23/6934
float estimate_nitrogen(float ec, float pH, float moisture) {
    float base_n;

    // Logarithmic response
    if (ec < 100)
        base_n = ec * 0.5f;  // very low EC = likely poor nitrogen
    else
        base_n = 100.0f * logf(ec / 100.0f + 1.0f);  // mg/L scale

    // Adjust for pH availability
    if (pH < 5.5 || pH > 8.0)
        base_n *= 0.6f;
    else if (pH < 6.0 || pH > 7.5)
        base_n *= 0.8f;

    // Moisture correction
    if (moisture < 20)
        base_n *= 0.7f;
    else if (moisture > 60)
        base_n *= 0.9f;

    // Cap if desired (typical soil N rarely exceeds 150 mg/L)
    if (base_n > 150.0f)
        base_n = 150.0f;

    return base_n; // mg/L
}

float estimate_phosphorus(float ec, float pH, float moisture) {
    float base_p = 10.0f; // Starting baseline

    // pH sensitivity (P locks in acidic/alkaline soils)
    if (pH < 5.5 || pH > 7.5)
        base_p *= 0.5f;
    else if (pH < 6.0 || pH > 7.0)
        base_p *= 0.75f;

    // Mild log-based EC contribution
    if (ec > 100)
        base_p += 10.0f * logf(ec / 100.0f + 1.0f);
    else
        base_p += ec * 0.1f;

    // Moisture mobility
    if (moisture < 25)
        base_p *= 0.75f;

    // Cap to reasonable field max (e.g., 50–60 mg/L)
    if (base_p > 60.0f)
        base_p = 60.0f;

    return base_p; // mg/L
}


float estimate_potassium(float ec, float pH, float moisture) {
    float base_k;

    // Use logarithmic growth for realistic ramp-up
    if (ec < 100)
        base_k = ec * 0.3f;  // very low EC, likely infertile
    else
        base_k = 150.0f * logf(ec / 100.0f + 1.0f);  // smooth curve

    // Apply pH correction
    if (pH < 5.5 || pH > 8.0)
        base_k *= 0.7f;
    else if (pH > 7.5)
        base_k *= 0.85f;

    // Moisture impact on ion mobility
    if (moisture < 20)
        base_k *= 0.6f;

    if (base_k > 2000.0f)
        base_k = 2000.0f;

    return base_k; // mg/L
}


//NPK Sensor Set Up

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
        ds18x20_measure(ONE_WIRE_BUS, DS18X20_ANY,  true);
        result = ds18b20_read_temperature(ONE_WIRE_BUS, DS18X20_ANY, &ds18_temperature);
        if (result == ESP_OK) {
            data.temperatureDS18B20 = ds18_temperature;
        } else {
            ESP_LOGW("SENSOR_MODULE", "Failed to read from DS18B20, error: %s", esp_err_to_name(result));
            data.temperatureDS18B20 = -127.0;  // Or some invalid value to signify failure
        }

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

        read_value_485("Humidity",     humi,  true,  "%", &rs485_moisture);
        // read_sensor("Temperature",  temp,  true,  "deg.C");
        read_value_485("Conductivity", cond,  false, "uS/cm", &rs485_ec);
        read_value_485("pH",           phph,  true,  "", &rs485_ph);
        read_value_485("Nitrogen",     nitro, false, "mg/L", &rs485_n);
        read_value_485("Phosphorus",   phos,  false, "mg/L", &rs485_p);
        read_value_485("Potassium",    pota,  false, "mg/L", &rs485_k);
        // read_value_485("Salinity",     sali,  false, "g/L");
        //read_value_485("TDS",          tds,   false, "mg/L", &rs485_n);

        data.nitro = estimate_nitrogen(rs485_ec, rs485_ph, rs485_moisture);
        data.phos = estimate_phosphorus(rs485_ec, rs485_ph, rs485_moisture);
        data.pota =estimate_potassium(rs485_ec, rs485_ph, rs485_moisture);

        //data.nitro = rs485_n;
        //data.phos = rs485_p;
        //data.pota = rs485_k;
        data.ph = rs485_ph;



        // Print data in CSV format
        // Serial.print(currMillis); Serial.print(", ");
        // Serial.print(data.moisturePercent); Serial.print(", ");
        // Serial.print(data.humidity); Serial.print(", ");
        // Serial.print(data.temperatureDS18B20); Serial.print(", ");
        // Serial.println(data.temperatureDHT);
    }
    ESP_LOGI("SENSOR_MODULE", "Timestamp: %lu ms | Moisture: %.2f%% | Humidity: %.2f%% | Soil Temp: %.2f°C | Air Temp: %.2f°C | Soil pH: %.2f | Nitrogen: %.2f | Phosphorus: %.2f | Potassium: %.2f", 
                 data.currentMillis, data.moisturePercent, data.humidity, data.temperatureDS18B20, data.temperatureDHT, data.ph, data.nitro, data.phos, data.pota);
    append_csv_row(data.currentMillis, data.moisturePercent, data.humidity, data.temperatureDS18B20, data.temperatureDHT, data.ph, data.nitro, data.phos, data.pota);
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

bool stopLogging = false;
char csv_buffer[4096];  // ~4KB buffer for storing session
size_t buffer_index = 0;

// CSV Portion - Probaly should be moved to the sensor processing
void append_csv_row(uint32_t timestamp, float moisture, float humidity,
                     float soil_temp, float air_temp,
                    float ph, float nitro, float phos, float pota){  
    buffer_index += snprintf(&csv_buffer[buffer_index],
        sizeof(csv_buffer) - buffer_index,
        "%lu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
        timestamp, moisture, humidity, soil_temp, air_temp, ph, nitro, phos, pota);

    // Avoid buffer overflow
    if (buffer_index >= sizeof(csv_buffer) - 100) {
        ESP_LOGE(TAG1, "CSV buffer is almost full!");
    }
}

void save_csv_to_flash(char *name) {
    char base_path[128];
    char file_path[256];
    int suffix = 0;
    FILE *f = NULL;

    // Step 1: Define the base file path pattern
    if (name == NULL || strlen(name) == 0) {
        snprintf(base_path, sizeof(base_path), "/csv_logs/s_log");
    } else {
        snprintf(base_path, sizeof(base_path), "/csv_logs/%s_s_log", name);
    }

    // Step 2: Generate a unique file path
    do {
        if (suffix == 0) {
            snprintf(file_path, sizeof(file_path), "%s.csv", base_path);
        } else {
            snprintf(file_path, sizeof(file_path), "%s_%d.csv", base_path, suffix);
        }

        f = fopen(file_path, "r");
        if (f != NULL) {
            fclose(f);
            suffix++;
        }
    } while (f != NULL && suffix < 100);  // safety limit

    // Step 3: Open the unique file for writing
    f = fopen(file_path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG1, "Failed to open file for writing: %s", file_path);
        return;
    }

    // Step 4: Write header and buffer data
    fprintf(f, "Timestamp,Moisture,Humidity,SoilTemp,AirTemp,pH,Nitrogen,Phosphorus,Potassium\n");
    fwrite(csv_buffer, 1, buffer_index, f);
    fclose(f);
    ESP_LOGI(TAG1, "CSV file saved to %s", file_path);

    // Step 5: Clear the buffer
    memset(csv_buffer, 0, sizeof(csv_buffer));
    buffer_index = 0;
}


// /* Initialize SPIFFS */
// static esp_err_t init_spiffs(void)
// {
//     ESP_LOGI(TAG1, "Initializing SPIFFS");

//     esp_vfs_spiffs_conf_t conf = {
//         .base_path = "/csv_logs",
//         .partition_label = "csv_logs",
//         .max_files = 5,
//         .format_if_mount_failed = true
//     };

//     esp_err_t ret = esp_vfs_spiffs_register(&conf);
//     if (ret != ESP_OK) {
//         if (ret == ESP_FAIL) {
//             ESP_LOGE(TAG1, "Failed to mount or format filesystem");
//         } else if (ret == ESP_ERR_NOT_FOUND) {
//             ESP_LOGE(TAG1, "Failed to find CSV partition");
//         } else {
//             ESP_LOGE(TAG1, "Failed to initialize CSV SPIFFS (%s)", esp_err_to_name(ret));
//         }
//         return ESP_FAIL;
//     }

//     size_t total = 0, used = 0;
//     ret = esp_spiffs_info("csv_logs", &total, &used);
//     if (ret != ESP_OK) {
//         ESP_LOGE(TAG1, "Failed to get CSV SPIFFS partition information (%s)", esp_err_to_name(ret));
//         return ESP_FAIL;
//     } else {
//         ESP_LOGI(TAG1, "Partition size: total: %d, used: %d", total, used);
//     }
    
//     return ESP_OK;
// }

// CSV Portion 
void read_csv_from_flash(char *name) {
    char base_path[128];
    char versioned_path[256];
    int suffix = 0;
    FILE *f = NULL;

    // Generate base path
    if (name == NULL || strlen(name) == 0) {
        snprintf(base_path, sizeof(base_path), "/csv_logs/s_log");
    } else {
        snprintf(base_path, sizeof(base_path), "/csv_logs/%s_s_log", name);
    }

    // Read unversioned file first
    snprintf(versioned_path, sizeof(versioned_path), "%s.csv", base_path);
    f = fopen(versioned_path, "r");

    if (f != NULL) {
        ESP_LOGI(TAG1, "Reading: %s", versioned_path);
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            ESP_LOGI(TAG1, "CSV: %s", line);
        }
        fclose(f);
    } else {
        ESP_LOGW(TAG1, "File not found: %s", versioned_path);
    }

    // Now check for versioned files like _1.csv, _2.csv, etc.
    suffix = 1;
    while (suffix < 100) {
        snprintf(versioned_path, sizeof(versioned_path), "%s_%d.csv", base_path, suffix);
        f = fopen(versioned_path, "r");

        if (f == NULL) {
            break;  // No more files found
        }

        ESP_LOGI(TAG1, "Reading: %s", versioned_path);
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            ESP_LOGI(TAG1, "CSV: %s", line);
        }
        fclose(f);
        suffix++;
    }

    if (suffix == 1 && f == NULL) {
        ESP_LOGE(TAG1, "No CSV files found for base name: %s", base_path);
    }
}


// Define maint ask functions: To be used by main.c

void sensor_task(void *pvParameters) {
    sensor_init();
    onewire_reset(ONE_WIRE_BUS);
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



    sensor_start_time = esp_timer_get_time();
    if (sensor_task_handle == NULL) {
        xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, &sensor_task_handle);
        ESP_LOGI(TAG1, "Sensor task started.");
    } else {
        ESP_LOGW(TAG1, "Sensor task already created. Use toggle_sensor instead.");
    }
}

void toggle_sensor() {
    if (sensor_task_handle != NULL) {
        eTaskState state = eTaskGetState(sensor_task_handle);
        if (state == eSuspended) {
            vTaskResume(sensor_task_handle);
            ESP_LOGI(TAG1, "Sensor task resumed.");
        } else {
            vTaskSuspend(sensor_task_handle);
            ESP_LOGI(TAG1, "Sensor task suspended.");
        }
    } else {
        ESP_LOGW(TAG1, "Sensor task handle is NULL. Call start_sensor() first.");
    }
}


void stop_sensor(char *name) {
    if (sensor_task_handle != NULL) {
        if (name == NULL || strlen(name) == 0) {
            ESP_LOGW(TAG1, "Provided name is empty before deleting task.");
        } else {
            ESP_LOGI(TAG1, "Stopping sensor task with name: %s", name);
        }

        // Save file
        save_csv_to_flash(name);
        read_csv_from_flash(name);
        // Delete task
        vTaskDelete(sensor_task_handle);
        sensor_task_handle = NULL;


    } else {
        ESP_LOGW(TAG1, "No sensor task to delete.");
    }
}


