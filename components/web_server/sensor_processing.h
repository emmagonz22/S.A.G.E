#ifndef SENSOR_PROCESSING_H
#define SENSOR_PROCESSING_H

#include "dht/dht.h"
#include "dht.h"

#include "ds18x20.h"
#include "ds18x20/ds18x20.h"
#include "onewire.h"
#include "onewire/onewire.h"
#include <driver/adc.h>


//#include <OneWire.h>
//#include <DallasTemperature.h>

// Pin Definitions
#define DHTPIN 4               // Pin for DHT sensor
#define DHTTYPE DHT11          // DHT 11 sensor type
#define ONE_WIRE_BUS GPIO_NUM_5         // Pin for DS18B20 sensor
//#define DS18B20_GPIO 5

// Analog Sensors
#define ANALOG_SENSOR_PIN_MOISTURE ADC1_CHANNEL_6  // GPIO34 is ADC1 Channel 6
#define MOISTURE_PIN 34        // Pin for soil moisture sensor
#define AIR_VALUE 2500            // Raw value representing air for moisture sensor
#define WATER_VALUE 1000       // Raw value representing water for moisture sensor

//Struct for Data
typedef struct {
    unsigned long currentMillis;
    float moisturePercent;
    float humidity;
    float temperatureDS18B20;
    float temperatureDHT;
} SensorData;

// Sensor Instances
// extern DHT dht;
// extern OneWire oneWire;
// extern DallasTemperature ds18b20;

// Function Declarations
void sensor_init();
SensorData read_sensors();
float map_value(float x, float in_min, float in_max, float out_min, float out_max);
float constrain_value(float value, float min_val, float max_val);

void append_csv_row(uint32_t timestamp, float moisture, float humidity, float soil_temp, float air_temp);
void save_csv_to_flash(char *name);
void read_csv_from_flash(char *name);
static esp_err_t init_spiffs(void);

void sensor_task(void *pvParameters);
void start_sensor();
void stop_sensor();
#endif // SENSOR_PROCESSING_H
