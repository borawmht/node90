# Node90 Firmware

MCU: PIC32MX795F512H

PHY: LAN8720A

EEPROM: 25LC1024

FLASH: SST26VF064B

# Project

Setup using Code Configurator v5.5.1 and Harmony 3 1.5.5. 

Build with XC32 4.60.

## Structure

Bootloader (0x9D000000 - 0x9D00FFFF) ← 64KB for bootloader

Application (0x9D010000 - 0x9D07FFFF) ← 448KB for your app

## Modules
- core 3.15.3
- csp 3.22.6
- FreeRTOS-Kernel 10.5.1
- net 3.14.2
- zlib 1.3.1
- wolfssl 5.4.0 (build errors if using 5.7.0)
- crypto 3.8.2

## Memory Usage
- net_pres + wolfssl provider = 40KB
- wolfSSL = minimal 50KB
- dns = 15KB
- tcp + crypto = 100KB


http client crypto minimal = 297KB 55%
http client minimal components (tcp+crypto) = 397KB 74%
https client all components (net_pres + wolfssl) = 509KB 95%

not enough program flash for https client using wolfssl.

Try secure element like ATECC608C for TLS handshake.
cryptoauthlib
https://github.com/wolfSSL/microchip-atecc-demos
https://github.com/wolfSSL/microchip-atecc-demos/blob/master/wolfssl_pic32mz_curiosity/wolfssl_client/firmware/src/app.c

Actually, I adjusted the wolfssl configuration for minimal and removed the harmony crypto lib. This is much smaller.

http client wolfssl minimal 273KB 51%

No, I am wrong. This did not have the ssl included. When I include ssl it is much larger

https client wolfssl 546KB >100%.

There are several large ssl components (>227.8KB)
- internal.c = 75.0KB
- ssl.c = 42.5KB
- asn.c = 37.6KB
- ecc.c = 27.2KB
- tfm.c = 21.9KB
- aes.c = 9.5KB
- rsa.c = 8.5KB
- dh.c = 5.6KB

Remove some components (>166.7KB)
- internal.c = 59.1KB
- ssl.c = 39.1KB
- asn.c = 29.6KB
- tfm.c = 21.3KB
- aes.c = 9.5KB
- rsa.c = 8.1KB

https client program size:

wolfssl: 473,608 bytes 88% FLASH, 86,075 bytes 66% RAM
bearssl: 

# Tools

```
# Create releases
python tools/create_release.py patch          # 1.0.0 -> 1.0.1
python tools/create_release.py minor          # 1.0.0 -> 1.1.0
python tools/create_release.py major          # 1.0.0 -> 2.0.0

# Create releases for specific components
python tools/create_release.py bootloader patch
python tools/create_release.py application minor

# Flash releases
python tools/flash.py bootloader 1.2.3
python tools/flash.py application 1.2.3
python tools/flash.py merged release 1.2.3

# List available releases
python tools/flash.py list

# Using make targets
make release-patch
make flash-merged release 1.2.3
make list-releases
```