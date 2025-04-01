#include "sensor_processing.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <Arduino.h>
#include "dht/dht.h"
#include "dht.h"

#include "ds18x20.h"
#include "ds18x20/ds18x20.h"
#include "onewire.h"
#include "onewire/onewire.h"

#include <driver/adc.h>

// Initialize sensor instances
// DHT dht(DHTPIN, DHTTYPE);
// OneWire oneWire(ONE_WIRE_BUS);
// DallasTemperature ds18b20(&oneWire);

// Timer variables
unsigned long prevMillis = 0;
const long interval = 1000; // 1-second interval for reading data

// Store Sensor Values
float dht_humidity, dht_temperature, ds18_temperature;

void sensor_init() {
    // ds18b20.begin();
    // pinMode(MOISTURE_PIN, INPUT);

    // // Print CSV header (this will be printed once at startup)
    // Serial.println("Timestamp (ms), Soil Moisture (%), Air Humidity (%), Soil Temp (°C), Air Temp (°C)");
    adc1_config_width(ADC_WIDTH_BIT_12);  // 12-bit resolution (0 - 4095)
    adc1_config_channel_atten(ANALOG_SENSOR_PIN_MOISTURE, ADC_ATTEN_DB_11); // Full voltage range (0-3.3V)
}

SensorData read_sensors() {
    SensorData data;
    int raw_value;

    unsigned long currMillis = millis();
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


