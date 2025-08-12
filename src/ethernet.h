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

#define PROTO_TCP  6
#define PROTO_UDP 17

// Packet layer structs for clear packet construction
#pragma pack(push, 1)  // Ensure no padding between struct members

// Ethernet header (14 bytes)
typedef struct {
    uint8_t dst_mac[6];      // Destination MAC address
    uint8_t src_mac[6];      // Source MAC address
    uint16_t ethertype;      // EtherType (0x0800 for IPv4)
} ethernet_header_t;

typedef struct {
    ethernet_header_t header;
    //uint8_t payload[];
} ethernet_packet_t;

// IP header (20 bytes)
typedef struct {
    uint8_t version_ihl;     // Version (4 bits) + Header Length (4 bits)
    uint8_t tos;             // Type of Service
    uint16_t total_length;   // Total packet length
    uint16_t identification; // Identification
    uint16_t flags_offset;   // Flags (3 bits) + Fragment Offset (13 bits)
    uint8_t ttl;             // Time to Live
    uint8_t protocol;        // Protocol (17 for UDP)
    uint16_t checksum;       // IP header checksum
    uint8_t src_ip[4];       // Source IP address
    uint8_t dst_ip[4];       // Destination IP address
} ip_header_t;

typedef struct {
    ethernet_header_t eth;
    ip_header_t header;
    //uint8_t payload[];
} ip_packet_t;

// UDP header (8 bytes)
typedef struct {
    uint16_t src_port;       // Source port
    uint16_t dst_port;       // Destination port
    uint16_t length;         // UDP packet length (header + data)
    uint16_t checksum;       // UDP checksum
} udp_header_t;

typedef struct {
    ethernet_header_t eth;
    ip_header_t ip;
    udp_header_t header;
    //uint8_t payload[];
} udp_packet_t;

#pragma pack(pop)  // Restore default packing

void ethernet_init();
bool ethernet_hasIP(void);
bool ethernet_linkUp(void);
uint8_t * ethernet_getIPAddress(void);
uint8_t * ethernet_getMACAddress(void);
uint8_t * ethernet_getSubnetMask(void);
uint8_t * ethernet_getGateway(void);
uint8_t * ethernet_getBroadcastAddress(void);
char * ethernet_getIPAddressString(void);
char * ethernet_getMACAddressString(void);
char * ethernet_getSubnetMaskString(void);
char * ethernet_getGatewayString(void);
char * ethernet_getBroadcastAddressString(void);
bool ethernet_send(const uint8_t * buf, uint32_t len);
bool ethernet_send_to(const uint8_t * dstMac, const uint8_t * payload, uint32_t payloadLen, uint16_t etherType);

#endif /* ETHERNET_H */
