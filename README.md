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


To test the website locally in the computer install python 3 then run the following command inside the web folder:

```
python -m http.server 8000
```

To run in the EDP32 use the following command 

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

