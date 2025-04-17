# S.A.G.E

## Getting started

Install IDF toolchain
Windows
```
https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/windows-setup.html
```
Linux & Macos
```
https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/linux-macos-setup.html
```

### Dependencies
 
 idf.py add-dependency "espressif/arduino-esp32^3.0.2
Given we are using Arduino Libraries we need the added dependencies.
Warning! this might result in a CONFIG_FREERTOS_HZ error. You can consult the debugging section for help.
 

To test the website locally in the computer install python 3 then run the following command inside the web folder:

```
python -m http.server 8000
```

To run in the ESP32 use the following command 

(These operations should be done through the ESP-IDF terminal)

```
idf.py set-target esp32

idf.py build

idf.py flash

idf.py monitor
```

To configure IDF 
```
idf.py menuconfig
```

##Debugging
If during set up: "idf.py build fails", you might be met with the following error:
```
FAILED: partition_table/partition-table.bin C:/Users/UserHere/esp/S.A.G.E/build/partition_table/partition-table.bin
Partitions tables occupies 2.1MB of flash (2162688 bytes) which does not fit in configured flash size 2MB. Change the flash size in menuconfig under the 'Serial Flasher Config' menu.
[5/998] Generating C:/Users/UserHere/esp/S.A.G.E/build/esp-idf/esp_system/ld/sections.ld.in linker script...
ninja: build stopped: subcommand failed.
ninja failed with exit code 1, output of the command is in the C:\Users\UserHere\esp\S.A.G.E\build\log\idf_py_stderr_output_31648 and C:\Users\UserHere\esp\S.A.G.E\build\log\idf_py_stdout_output_31648
```

In which case ensure, that you are using the appropiate flash size for your ESP32.

In case of the ESP32 WROOM32, it has a flash size of 4MB. Which can be changed through the MenuConfig

Serial Flasher Config → Flash Size


 CONFIG_FREERTOS_HZ Error:

 ```
CMake Error at managed_components/espressif__arduino-esp32/CMakeLists.txt:325 (message):
  esp32-arduino requires CONFIG_FREERTOS_HZ=1000 (currently 100)


-- Configuring incomplete, errors occurred!
ninja: error: rebuilding 'build.ninja': subcommand failed
```

This error occurs because of the HZ in the built in files, the only way to fix it permanently is to go to the definition file and changing the 1000 to a 100.


