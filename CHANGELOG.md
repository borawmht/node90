# Changelog

Node90 Firmware

# 1.6.0
- add pwm, i2c, io_expander, dac, adc
- add actuator mode, dim_els, cuv_enable
- add output enable protection delay
- add send event and coap send message
- add execute event
- add send status and tag request
- fix ethernet send packet allocation
- reduce resource str sizes from 2048 to 1014 to match coap payload size 1024
- fix json print allocation stack overflow
- remove coap_task, instead call coap_packet_queue_task from app_10ms_task
- reduce FreeRTOS heap from 44000 to 30000, 

# 1.5.0
- add actuators, sensors, policy, and event resources

# 1.4.4
- fix issue coap response during firmware update, disable ethernet_send if http_stream is open
- revert app stack depth 4KB to 2KB and FreeRTOS heap 48000 to 44000

# 1.4.3
- add LED_STAT_Toggle() during firmware update
- change WDTPS PS8192 8 seconds timeout
- remove disable interrupts in freertos_hooks

# 1.4.2
- fix binary size +1

# 1.4.1
- add task yields during firmware update processes
- increase app stack depth 2KB to 4KB
- increase FreeRTOS heap 44000 to 48000

# 1.4.0
- fix create_realease.py hex_to_bin start and end memory addresses for larger bootloader size
- add node90_latest.bin during create_realease.py
- fix storage_getStr() null terminator
- add version and ota resources

# 1.3.0
- change bootloader size to 32KB

# 1.2.0
- refactor http/https client and firmware update download
- fix command backspace key 0x08 or 0x7F and delete key vt100 escape sequence
- fix create_realease.py hex_to_bin start and end memory addresses

# 1.1.0
- add download firmware

# 1.0.1
- test release

# 1.0.0
- initial release