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
// Pins for the RS485 NPKPH sensor
#define TXD_PIN 16
#define RXD_PIN 17
#define RE_PIN  18
#define UART_PORT UART_NUM_2
// Addresses for the sensor
static const uint8_t humi[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0xe84, 0x0a};
static const uint8_t temp[] = {0x01, 0x03, 0x00, 0x01, 0x00, 0x01, 0xd5, 0xca};
static const uint8_t cond[] = {0x01, 0x03, 0x00, 0x02, 0x00, 0x01, 0x25, 0xca};
static const uint8_t phph[] = {0x01, 0x03, 0x00, 0x03, 0x00, 0x01, 0x74, 0x0a};
static const uint8_t nitro[] = {0x01, 0x03, 0x00, 0x04, 0x00, 0x01, 0xec5, 0xcb};
static const uint8_t phos[] = {0x01, 0x03, 0x00, 0x05, 0x00, 0x01, 0xe94, 0x0b};
static const uint8_t pota[] = {0x01, 0x03, 0x00, 0x06, 0x00, 0x01, 0xe64, 0x0b};
static const uint8_t sali[] = {0x01, 0x03, 0x00, 0x07, 0x00, 0x01, 0xe35, 0xcb};
static const uint8_t tds[] = {0x01, 0x03, 0x00, 0x08, 0x00, 0x01, 0xe05, 0xc8};

static const char *TAG1 = "SENSOR_MODULE";
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
    float phos;
    float pota;
    float nitro;
    float ph;
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

void append_csv_row(uint32_t timestamp, float moisture, float humidity, float soil_temp, float air_temp, float ph, float nitro, float phos, float pota);
void save_csv_to_flash(char *name);
void read_csv_from_flash(char *name);
// static esp_err_t init_spiffs(void);
void write_test_csv_to_flash(void);
void write_test_csv_to_flash_1(void);
void write_test_csv_to_flash_2(void);

void sensor_task(void *pvParameters);
void start_sensor();
void stop_sensor();
void toggle_sensor();
#endif // SENSOR_PROCESSING_H
