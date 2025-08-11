/*
* Ethernet initialization and management
* ethernet.h
* created by: Brad Oraw
* created on: 2025-08-10
*/

#ifndef ETHERNET_H
#define ETHERNET_H

#include "stdbool.h"
#include "stdint.h"

void ethernet_init();
bool ethernet_hasIP(void);
bool ethernet_linkUp(void);
uint8_t * ethernet_getIPAddress(void);
uint8_t * ethernet_getMACAddress(void);
bool ethernet_send(const uint8_t * buf, uint32_t len);
bool ethernet_send_to(const uint8_t * dstMac, const uint8_t * payload, uint32_t payloadLen, uint16_t etherType);

#endif /* ETHERNET_H */
