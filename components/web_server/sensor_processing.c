#include "sensor_processing.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <Arduino.h>
#include "dht/dht.h"
#include "dht.h"

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
}

SensorData read_sensors() {
    SensorData data;

    // if (onewire_reset((gpio_num_t)ONE_WIRE_PIN)) {
    //     Serial.println("1-Wire bus initialized");
    // } else {
    //     Serial.println("1-Wire bus initialization failed!");
    //     return;
    // }

    unsigned long currMillis = millis();
    if (currMillis - prevMillis >= interval) {
        prevMillis = currMillis; // Reset timer

        // Read DHT11 sensor
        esp_err_t result = dht_read_float_data(DHT_TYPE_DHT11, DHTPIN, &dht_humidity, &dht_temperature);
        data.temperatureDHT = dht_temperature;
        data.humidity = dht_humidity;
        
        // Read DS18B20 sensor
        // esp_err_t result = ds18b20_read_temperature(ONE_WIRE_BUS, onewire_addr_t addr, float *ds18_temperature);

        // ds18b20.requestTemperatures();
        // data.temperatureDS18B20 = ds18b20.getTempCByIndex(0);
        
        // Read Soil Moisture Sensor (analog)
        // int moistureRaw = analogRead(MOISTURE_PIN);

        // // Convert raw value to percentage
        // data.moisturePercent = map(moistureRaw, AIR_VALUE, WATER_VALUE, 0, 100);
        // data.moisturePercent = constrain(data.moisturePercent, 0, 100); // Keep within range

        // data.currentMillis = currMillis;

        // Print data in CSV format
        // Serial.print(currMillis); Serial.print(", ");
        // Serial.print(data.moisturePercent); Serial.print(", ");
        // Serial.print(data.humidity); Serial.print(", ");
        // Serial.print(data.temperatureDS18B20); Serial.print(", ");
        // Serial.println(data.temperatureDHT);
    }
    
    return data;
}

