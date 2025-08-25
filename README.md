# Node90 Firmware

MCU: PIC32MX795F512H

PHY: LAN8720A

EEPROM: 25LC1024

FLASH: SST26VF064B

# Project

Setup using Code Configurator v5.5.1 and Harmony 3 1.5.5. 

Build with XC32 4.60.

## Structure

Bootloader (0x9D000000 - 0x9D007FFF) ← 32KB for bootloader

Application (0x9D008000 - 0x9D07FFFF) ← 480KB for app

## Modules
- core 3.15.3
- csp 3.22.6
- FreeRTOS-Kernel 10.5.1
- net 3.14.2

## Memory Usage

http client 273KB 51%

https client program size:

wolfssl: 
- 473,608 bytes 88% FLASH, 86,075 bytes 66% RAM

bearssl: 
- 377,764 bytes 70% FLASH, 72,767 bytes 56% RAM init full
- 363,516 bytes 68% FLASH, 72,767 bytes 56% RAM w/o ecdsa
- 318,684 bytes 59% FLASH, 73,047 bytes 56% RAM knownkey rsa
- 326,176 bytes 61% FLASH, 72,791 bytes 56% RAM minimal rsa
- 322,712 bytes 60% FLASH, 74,387 bytes 57% RAM custom no validation rsa

# Tools

```
# Create releases
# Assume linux, make
python tools/create_release.py patch          # 1.0.0 -> 1.0.1
python tools/create_release.py minor          # 1.0.0 -> 1.1.0
python tools/create_release.py major          # 1.0.0 -> 2.0.0

# Release using shell script
release.sh patch
release.sh minor
release.sh major
```

```
# Flash using shell script
# Assume linux, MPLAB IPE 6.15, ICD4
flash.sh         # flash unified bootloader hex from bootloader project
flash.sh latest  # flash lastest merged hex from release directory
flash.sh 1.0.1   # flash particular version merged hex from release directory
```