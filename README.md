# DP Adam Zverbik
The aim of the thesis is to design and implement hardware module made of __ESP32 Microcontroller board__ and MEMS microphone __INMP 441__. Hardware module will analyze and recognize sound signals within smart home solution. This solution uses open-source library [ESPHome](https://esphome.io/)

ESPHome is installed as an addon within [Home Assistant](https://www.home-assistant.io/)

## HW used

__Microcontroller board__:
[ESP32 DEVKIT V1](https://dratek.cz/arduino/1581-esp-32s-esp32-esp8266-development-board-2.4ghz-dual-mode-wifi-bluetooth-antenna-module.html)

__Microphone__:
[INMP441](https://www.laskakit.cz/inmp441-modul-i2s-mikrofonu/)

## Wiring

|INMP PIN   | ESP32 PIN
|-----------|-----------
|WS         | D22
|SCK        | D26
|SD         | D21
|VDD        | 3V3
|GND        | GND
|L/R        | GND

## Install
In this thesis ESPHome was used within Home Assistant, but this solution can be used as ESPHome solution only.

1. Install [Home Assistant (VM or Physically)](https://www.home-assistant.io/installation/)
2. Install [ESPHome as HA addon](https://esphome.io/guides/getting_started_hassio.html)
3. Install some ssh terminal from HA addons (terminal inside Studio Code Server is also an option)
4. Open the terminal (or connect remotely via ssh)
5. Run commands:
```
$ cd /config/esphome/
```
```
$ git init
```
```
$ git remote add origin https://gitlab.nsoric.com/mtf/esp/sound-analysis.git
```
```
$ git pull origin main
```
(You may need to configure SSH Key in HA - in that case run `$ ssh-keygen` and copy generated SSH key to gitlab(github, or other) user settings)

## Upload source code
In ESPHome UI (within HA) click on __EDIT__ button for individual hw modules. Select __Install__ and for first installation choose __Manual__. Binary file will be downloaded to your PC. Then you can plug the ESP32 to the PC with micro USB cable and upload downloaded binary file using [ESPHome-Flasher software](https://github.com/esphome/esphome-flasher/releases). After the first manual upload, code can be uploaded wirelessly (Over The Air).

## Structure

In the root (`/config/esphome/`) there are two YAML config files:
1. `fourier-analyzer.yaml` - containing configuration for module using algorithm of Fast Fourier Transform for sound analysis
- Corresponding header files to this hw module are in `fourier-analysis` folder 
2. `correlation-analyzer.yaml` - containing configuration for module using algorithm of Cross-Correlations for sound analysis
- Corresponding header files to this hw module are in `correlation-analysis` folder 
