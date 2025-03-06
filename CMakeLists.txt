# Minimum required CMake version for ESP-IDF
cmake_minimum_required(VERSION 3.16)

# Set ESP-IDF target before project command
set(ENV{IDF_TARGET} "esp32")
# Include ESP-IDF project include first
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define the project after including ESP-IDF project.cmake
project(S.A.G.A)

# Add extra component directories if needed
set(EXTRA_COMPONENT_DIRS components)

# Create SPIFFS partition image for web content
spiffs_create_partition_image(storage web FLASH_IN_PROJECT)