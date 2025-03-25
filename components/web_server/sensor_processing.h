#ifndef SENSOR_PROCESSING_H
#define SENSOR_PROCESSING_H

#include "dht/dht.h"
#include "dht.h"

//#include <OneWire.h>
//#include <DallasTemperature.h>

// Pin Definitions
#define DHTPIN 4               // Pin for DHT sensor
#define DHTTYPE DHT11          // DHT 11 sensor type
#define ONE_WIRE_BUS 5         // Pin for DS18B20 sensor
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

#endif // SENSOR_PROCESSING_H
