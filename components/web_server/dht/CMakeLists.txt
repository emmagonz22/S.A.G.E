if(${IDF_TARGET} STREQUAL esp32)
    set(req driver freertos log esp_idf_lib_helpers)
elseif(${IDF_TARGET} STREQUAL esp8266)
    set(req esp8266 freertos log esp_idf_lib_helpers)
endif()

idf_component_register(
    SRCS dht.c
    INCLUDE_DIRS .
    REQUIRES esp_idf_lib_helpers ${req} 
)

# Ensure ESP8266 specific includes are available
if(${IDF_TARGET} STREQUAL esp8266)
    target_include_directories(${COMPONENT_LIB} PRIVATE ${IDF_PATH}/components/esp8266/include)
endif()
