idf_component_register(SRCS "sensor_processing.c" "web_server.c" "dht/dht.c" "onewire/onewire.c" "ds18x20/ds18x20.c"
                       INCLUDE_DIRS "." "dht" "ds18x20" "onewire"
                       REQUIRES esp_http_server spiffs vfs driver esp_timer)
