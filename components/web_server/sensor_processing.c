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

        data.moisturePercent = rs485_moisture;
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
    bool file_found = false;

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
        file_found = true;
        ESP_LOGI(TAG1, "Reading: %s", versioned_path);
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            ESP_LOGI(TAG1, "CSV: %s", line);
        }
        fclose(f);
    }

    // Check versioned files i.e _1.csv, _2.csv, etc.
    suffix = 1;
    while (suffix < 100) {
        snprintf(versioned_path, sizeof(versioned_path), "%s_%d.csv", base_path, suffix);
        f = fopen(versioned_path, "r");

        if (f == NULL) {
            break;
        }

        file_found = true;
        ESP_LOGI(TAG1, "Reading: %s", versioned_path);
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            ESP_LOGI(TAG1, "CSV: %s", line);
        }
        fclose(f);
        suffix++;
    }

    if (!file_found) {
        ESP_LOGE(TAG1, "No CSV files found for base name: %s", base_path);
    }
}



// Define maint ask functions: To be used by main.c
static bool sensor_initialized = false;

// This is because of UART, multiple calls cause issues.
void sensor_init_safe() {
    if (!sensor_initialized) {
        sensor_init(); 
        sensor_initialized = true;
    }
}

void sensor_task(void *pvParameters) {
    sensor_init_safe();
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


void write_test_csv_to_flash() {
    const char *test_csv_path = "/csv_logs/Test_s_log.csv";
    FILE *f = fopen(test_csv_path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG1, "Failed to open %s for writing", test_csv_path);
        return;
    }

    const char *csv_data =
    "Timestamp,Moisture,Humidity,SoilTemp,AirTemp,pH,Nitrogen,Phosphorus,Potassium\n"
    "1747015860,29.0,48.59,20.81,23.24,5.51,49.94,30.03,39.45\n"
    "1747015920,29.19,50.07,20.55,22.74,5.53,49.64,29.66,38.51\n"
    "1747015980,28.1,49.5,20.89,22.35,6.06,49.23,30.76,39.84\n"
    "1747016040,28.02,49.21,20.81,23.06,7.0,48.83,31.57,40.69\n"
    "1747016100,28.24,49.24,20.48,23.12,7.02,47.63,31.27,41.71\n"
    "1747016160,29.61,48.93,21.07,24.24,6.65,47.21,31.0,41.46\n"
    "1747016220,29.55,49.44,22.42,24.92,7.5,47.35,31.58,41.27\n"
    "1747016280,30.72,50.14,23.66,23.86,6.6,46.91,31.66,40.76\n"
    "1747016340,30.38,50.35,24.29,23.43,6.36,45.83,32.32,39.55\n"
    "1747016400,31.09,50.44,22.89,23.82,6.82,46.51,31.18,40.04\n"
    "1747016460,29.95,49.38,24.36,23.38,7.5,45.37,30.26,40.14\n"
    "1747016520,31.14,47.99,25.51,24.61,7.5,46.65,31.08,41.01\n"
    "1747016580,30.52,48.02,25.92,23.16,7.5,46.5,30.11,39.54\n"
    "1747016640,29.56,49.07,27.18,24.05,7.5,46.42,29.06,40.2\n"
    "1747016700,30.38,47.79,27.03,23.81,7.5,46.64,28.96,41.67\n"
    "1747016760,29.91,46.62,26.06,24.93,6.11,47.35,27.97,40.24\n"
    "1747016820,30.64,45.52,25.83,23.49,5.5,46.34,26.81,39.19\n"
    "1747016880,31.92,46.87,27.13,24.44,6.64,45.15,28.13,39.34\n"
    "1747016940,31.21,46.02,26.26,23.05,5.67,44.8,29.47,38.53\n"
    "1747017000,32.26,46.07,26.6,24.37,6.01,44.76,29.11,38.25\n"
    "1747017060,32.06,45.49,25.78,25.37,5.81,44.32,30.11,39.6\n"
    "1747017120,33.02,45.17,26.75,25.55,7.18,43.7,30.9,39.34\n"
    "1747017180,33.49,45.7,26.06,25.44,7.48,44.02,32.12,40.63\n"
    "1747017240,32.71,44.36,26.16,25.17,7.5,45.24,31.52,40.11\n"
    "1747017300,33.88,45.78,26.51,25.77,7.5,44.52,31.63,41.21\n"
    "1747017360,33.11,45.98,26.8,25.45,7.5,43.9,31.53,40.39\n"
    "1747017420,33.36,44.86,26.58,26.05,7.4,42.71,32.55,39.59\n"
    "1747017480,33.6,44.37,25.18,26.73,7.5,44.11,33.7,40.38\n"
    "1747017540,34.53,43.63,26.58,25.91,7.5,44.23,33.36,39.11\n"
    "1747017600,35.32,42.85,27.42,25.16,7.5,42.85,32.42,40.22\n"
    "1747017660,35.32,43.82,28.83,24.94,7.27,43.64,33.38,40.4\n"
    "1747017720,34.09,42.47,27.54,25.73,6.17,42.84,34.19,40.28\n"
    "1747017780,34.62,43.87,27.8,26.36,5.88,42.97,34.51,41.54\n"
    "1747017840,35.19,42.4,27.17,25.46,6.18,42.36,34.14,41.56\n"
    "1747017900,35.4,42.87,28.09,24.15,6.44,41.95,32.8,41.31\n"
    "1747017960,35.9,41.6,29.51,25.5,5.55,43.22,32.07,42.56\n"
    "1747018020,36.8,41.54,30.86,25.49,5.5,43.74,32.03,43.75\n"
    "1747018080,36.56,41.12,30.14,26.04,6.14,44.08,32.99,43.95\n"
    "1747018140,35.91,40.13,29.98,26.85,6.38,42.96,32.79,44.18\n"
    "1747018200,34.66,40.6,29.39,25.51,5.5,42.34,34.21,44.74\n"
    "1747018260,35.45,41.14,29.95,24.12,5.6,42.64,33.12,45.7\n"
    "1747018320,34.55,41.39,30.37,24.01,5.79,43.86,33.63,45.06\n"
    "1747018380,35.43,42.16,31.1,25.07,5.5,43.32,34.28,46.42\n"
    "1747018440,35.27,41.85,32.02,25.01,6.19,43.74,34.92,47.18\n"
    "1747018500,35.31,41.57,30.77,23.55,5.5,43.66,34.91,47.98\n"
    "1747018560,34.25,41.12,30.18,22.47,6.61,44.24,33.49,47.53\n"
    "1747018620,33.65,41.79,29.35,23.19,7.5,44.13,32.42,46.17\n"
    "1747018680,33.89,41.77,30.66,23.41,7.5,45.18,31.16,46.29\n"
    "1747018740,34.92,41.68,29.77,22.62,7.11,44.71,31.3,44.88\n"
    "1747018800,34.94,41.89,30.53,21.15,7.45,45.13,30.48,45.26\n";

    fwrite(csv_data, 1, strlen(csv_data), f);
    fclose(f);
    ESP_LOGI(TAG1, "Dummy Test_s_log.csv written to %s", test_csv_path);
}

void write_test_csv_to_flash_1() {
    const char *test_csv_path = "/csv_logs/Test_s_log_1.csv";
    FILE *f = fopen(test_csv_path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG1, "Failed to open %s for writing", test_csv_path);
        return;
    }

    const char *csv_data =
    "Timestamp,Moisture,Humidity,SoilTemp,AirTemp,pH,Nitrogen,Phosphorus,Potassium\n"
    "1747062159,39.47,51.49,27.16,23.07,6.28,29.39,21.13,58.58\n"
    "1747062219,40.83,50.27,27.41,23.89,6.77,29.06,22.58,57.55\n"
    "1747062279,41.32,49.26,27.81,25.14,5.64,27.64,23.34,57.79\n"
    "1747062339,40.9,49.66,26.36,26.24,6.35,28.77,22.42,57.95\n"
    "1747062399,41.75,49.18,27.14,26.11,6.01,27.35,23.14,57.27\n"
    "1747062459,40.38,48.59,28.25,24.84,6.9,28.38,24.18,58.63\n"
    "1747062519,39.99,48.22,28.98,25.92,8.32,27.66,24.41,59.16\n"
    "1747062579,38.88,48.57,29.11,26.94,8.23,26.42,24.28,59.22\n"
    "1747062639,39.27,49.61,29.65,27.9,8.5,25.85,23.51,59.29\n"
    "1747062699,39.2,50.93,30.52,27.56,8.5,25.39,22.94,58.04\n"
    "1747062759,39.54,50.33,29.06,28.71,8.5,24.77,22.23,57.6\n"
    "1747062819,40.74,51.44,29.96,29.62,8.5,25.49,22.15,58.99\n"
    "1747062879,41.65,51.99,29.95,28.96,8.5,24.22,22.76,60.41\n"
    "1747062939,41.34,53.4,29.11,28.2,8.5,24.24,22.06,60.8\n"
    "1747062999,41.9,53.87,29.86,27.59,7.84,24.11,22.65,60.23\n"
    "1747063059,40.86,52.74,30.17,28.79,6.43,25.56,24.06,58.87\n"
    "1747063119,39.9,54.24,29.37,27.57,7.25,24.27,23.92,58.26\n"
    "1747063179,41.15,53.87,29.52,26.45,5.86,24.46,24.53,59.71\n"
    "1747063239,41.11,54.95,29.11,25.48,5.5,25.63,24.39,59.65\n"
    "1747063299,41.26,54.89,29.73,24.82,5.94,25.33,23.54,58.67\n"
    "1747063359,40.39,55.36,29.52,25.35,6.45,24.11,23.6,59.08\n"
    "1747063419,39.54,54.24,29.47,24.66,7.91,25.47,22.77,58.87\n"
    "1747063479,40.99,55.35,29.52,24.58,7.79,24.32,23.16,59.16\n"
    "1747063539,39.9,54.74,28.88,23.83,7.41,25.49,21.76,59.78\n"
    "1747063599,40.64,53.41,27.47,22.8,7.63,25.26,22.28,59.84\n";

    fwrite(csv_data, 1, strlen(csv_data), f);
    fclose(f);
    ESP_LOGI(TAG1, "Dummy Test_s_log_1.csv written to %s", test_csv_path);
}

void write_test_csv_to_flash_2() {
    const char *test_csv_path = "/csv_logs/Test_s_log_2.csv";
    FILE *f = fopen(test_csv_path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG1, "Failed to open %s for writing", test_csv_path);
        return;
    }

    const char *csv_data =
"Timestamp,Moisture,Humidity,SoilTemp,AirTemp,pH,Nitrogen,Phosphorus,Potassium\n"
"1747062703,70.19,79.15,20.68,21.16,6.98,79.13,40.58,78.82\n"
"1747062763,71.26,80.3,20.73,19.77,5.62,79.45,41.78,77.83\n"
"1747062823,72.31,80.41,19.74,20.4,6.16,79.11,41.53,76.48\n"
"1747062883,73.19,79.49,18.4,21.17,5.5,78.25,42.61,76.16\n"
"1747062943,71.96,78.18,19.34,21.32,6.46,78.89,41.3,76.42\n"
"1747063003,70.51,77.17,19.56,22.17,7.23,79.19,41.3,75.93\n"
"1747063063,69.99,76.45,19.89,22.62,6.53,80.58,42.15,77.06\n"
"1747063123,70.13,77.74,20.43,22.91,5.57,79.19,41.8,76.99\n"
"1747063183,68.8,77.18,21.45,21.91,6.63,79.21,40.9,78.3\n"
"1747063243,68.42,77.77,21.46,22.74,6.53,78.28,39.53,77.31\n"
"1747063303,69.64,78.14,20.96,23.15,7.03,79.66,39.6,77.43\n"
"1747063363,69.2,78.6,21.42,21.72,6.63,78.53,40.43,77.57\n"
"1747063423,70.42,77.6,22.35,21.24,8.1,77.92,41.55,78.48\n"
"1747063483,70.11,77.88,22.99,21.46,8.5,76.65,41.88,79.67\n"
"1747063543,70.62,79.21,23.08,21.32,7.71,77.58,41.86,79.31\n"
"1747063603,71.68,79.58,22.58,20.11,7.22,77.59,42.08,78.19\n"
"1747063663,73.13,80.66,22.84,20.26,7.27,77.03,41.24,78.07\n"
"1747063723,73.09,81.53,22.29,20.67,8.14,76.57,42.36,77.94\n"
"1747063783,71.72,80.14,22.5,19.84,7.23,77.24,42.8,77.1\n"
"1747063843,73.07,80.77,23.16,18.79,7.81,77.49,43.1,76.75\n"
"1747063903,72.84,79.82,21.92,19.48,8.5,77.88,44.05,77.28\n"
"1747063963,72.77,80.26,21.6,18.11,8.5,78.14,44.89,76.77\n"
"1747064023,74.21,78.85,20.19,17.94,8.5,79.4,43.78,75.98\n"
"1747064083,75.23,79.23,19.96,17.39,8.5,80.13,44.24,77.39\n"
"1747064143,73.81,79.51,19.71,16.72,8.5,78.64,43.28,78.72\n";

    fwrite(csv_data, 1, strlen(csv_data), f);
    fclose(f);
    ESP_LOGI(TAG1, "Dummy Test_s_log_2.csv written to %s", test_csv_path);
}