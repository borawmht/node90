/*
* Ethernet initialization and management
* ethernet.c
* created by: Brad Oraw
* created on: 2025-08-10
*/

#include "ethernet.h"
#include "coap_server.h"
#include "definitions.h"
#include "config/default/library/tcpip/src/tcpip_private.h"
#include "third_party/rtos/FreeRTOS/Source/include/queue.h"

TCPIP_NET_HANDLE netH = NULL;
TCPIP_STACK_PROCESS_HANDLE packetHandlerHandle = NULL;

bool is_stack_ready = false;
bool has_ip = false;
bool is_ready = false;
bool link_up = false;

uint8_t mac_addr[6] = {0,0,0,0,0,0};
IPV4_ADDR ip_address = {0};

typedef struct {
    TCPIP_MAC_ETHERNET_HEADER header;
    uint8_t * payload;
} packet_t;

// Packet handler function type
typedef bool (*ethernet_packet_handler_t)(TCPIP_NET_HANDLE hNet, TCPIP_MAC_PACKET* rxPkt, uint16_t frameType, const void* hParam);

// Packet handler functions
bool ethernet_register_packet_handler(ethernet_packet_handler_t handler, const void* handlerParam);
bool ethernet_deregister_packet_handler(void);

// Ethernet packet handler
bool ethernet_packet_handler(TCPIP_NET_HANDLE hNet, TCPIP_MAC_PACKET* rxPkt, uint16_t frameType, const void* hParam) {
    // Process the incoming packet
    // frameType contains the Ethernet frame type (0x0800 for IPv4, 0x0806 for ARP, etc.)
    
    // Example: Check if it's an IPv4 packet
    if (frameType == 0x0800) {
        //SYS_CONSOLE_PRINT("ethernet: received IPv4 packet, length: %d\r\n", rxPkt->totTransportLen);
        
        // Access the packet data
        uint8_t* packetData = (uint8_t*)rxPkt->pMacLayer;
        uint32_t packetLen = rxPkt->totTransportLen;

        packet_t * packet = (packet_t *)packetData;
        // SYS_CONSOLE_PRINT("ethernet: packet type: %04x\r\n", TCPIP_Helper_ntohs(packet->header.Type));
        //SYS_CONSOLE_PRINT("ethernet: packet length: %d\r\n", packetLen);
        
        // Parse IP header to check if it's UDP
        if (packetLen >= 28) {  // Ethernet(14) + IP(20) + UDP(8) minimum
            uint8_t protocol = packetData[23];  // IP protocol field
            if (protocol == 17) {  // UDP protocol
                // Parse UDP header
                uint16_t src_port = TCPIP_Helper_ntohs(*(uint16_t*)(&packetData[34]));
                uint16_t dst_port = TCPIP_Helper_ntohs(*(uint16_t*)(&packetData[36]));
                
                // Check if it's a CoAP packet (port 5683)
                if (src_port == 5683 || dst_port == 5683) {
                    //SYS_CONSOLE_PRINT("ethernet: found CoAP packet\r\n");
                    
                    // Extract IP addresses
                    uint8_t src_ip[4], dst_ip[4];
                    memcpy(src_ip, &packetData[26], 4);
                    memcpy(dst_ip, &packetData[30], 4);
                    
                    // Extract UDP payload (CoAP data)
                    uint8_t* udp_payload = &packetData[42];
                    uint16_t udp_length = TCPIP_Helper_ntohs(*(uint16_t*)(&packetData[38])) - 8;  // UDP length - header
                    
                    // Queue the packet for processing instead of handling it directly
                    if (coap_queue_packet(udp_payload, udp_length, src_ip, dst_ip, src_port, dst_port, packet->header.SourceMACAddr.v)) {
                        //SYS_CONSOLE_PRINT("ethernet: CoAP packet queued\r\n");
                        // Return FALSE to let TCP/IP stack also process it
                        // This prevents double processing
                        // return false;
                        return true;
                    } else {
                        SYS_CONSOLE_PRINT("ethernet: failed to queue CoAP packet\r\n");
                    }
                }
            }
        }
        
        // Process other packet data here...
        
        // Return true if you processed the packet (TCP/IP stack won't process it further)
        // Return false if you want the TCP/IP stack to process it normally
        return false; // Let the stack handle it normally
    }
    
    // For other frame types, let the stack handle them
    return false;
}

// New packet handler registration function
bool ethernet_register_packet_handler(ethernet_packet_handler_t handler, const void* handlerParam) {
    if (netH == NULL) {
        SYS_CONSOLE_PRINT("ethernet: cannot register handler - network not initialized\r\n");
        return false;
    }
    
    if (packetHandlerHandle != NULL) {
        SYS_CONSOLE_PRINT("ethernet: packet handler already registered\r\n");
        return false;
    }
    
    packetHandlerHandle = TCPIP_STACK_PacketHandlerRegister(netH, handler, handlerParam);
    if (packetHandlerHandle == NULL) {
        SYS_CONSOLE_PRINT("ethernet: failed to register packet handler\r\n");
        return false;
    }
    
    SYS_CONSOLE_PRINT("ethernet: packet handler registered successfully\r\n");
    return true;
}

// New packet handler deregistration function
bool ethernet_deregister_packet_handler(void) {
    if (packetHandlerHandle == NULL) {
        SYS_CONSOLE_PRINT("ethernet: no packet handler registered\r\n");
        return false;
    }
    
    bool result = TCPIP_STACK_PacketHandlerDeregister(netH, packetHandlerHandle);
    if (result) {
        packetHandlerHandle = NULL;
        SYS_CONSOLE_PRINT("ethernet: packet handler deregistered successfully\r\n");
    } else {
        SYS_CONSOLE_PRINT("ethernet: failed to deregister packet handler\r\n");
    }
    
    return result;
}

void ethernet_services_init(void){
    // Initialize the services
    SYS_CONSOLE_PRINT("ethernet: services init\r\n");
    coap_server_init(); // Initialize CoAP server
    SYS_CONSOLE_PRINT("Free heap: %d bytes\r\n", xPortGetFreeHeapSize());
}

void ethernet_task(void *pvParameters){
    SYS_CONSOLE_PRINT("ethernet: start task\r\n");
    while(1){
        if(!is_stack_ready ){
            SYS_STATUS stack_status = TCPIP_STACK_Status(sysObj.tcpip);
            if(stack_status == SYS_STATUS_READY){
                SYS_CONSOLE_PRINT("ethernet: stack is ready\r\n");
                netH = TCPIP_STACK_IndexToNet(0);                  
                is_stack_ready = true;
                // Register the packet handler (equivalent to esp_eth_update_input_path)
                if(packetHandlerHandle == NULL){
                    ethernet_register_packet_handler(ethernet_packet_handler, NULL);
                }
            }
        }
        if(!is_ready && TCPIP_STACK_NetIsReady(netH)){
            SYS_CONSOLE_PRINT("ethernet: is ready\r\n");
            is_ready = true;            
            TCPIP_STACK_NetAddressMac(netH);
            memcpy(mac_addr, (uint8_t*)TCPIP_STACK_NetAddressMac(netH), 6);
            SYS_CONSOLE_PRINT("ethernet: MAC address: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                    mac_addr[0],mac_addr[1],mac_addr[2],mac_addr[3],mac_addr[4],mac_addr[5]);            
        }
        if(!link_up && TCPIP_STACK_NetIsLinked(netH)){
            SYS_CONSOLE_PRINT("ethernet: link up\r\n");
            link_up = true;
        }
        if(link_up && !TCPIP_STACK_NetIsLinked(netH)){
            SYS_CONSOLE_PRINT("ethernet: link down\r\n");
            link_up = false;
        }
        if(ip_address.Val != TCPIP_STACK_NetAddress(netH)){
            ip_address.Val = TCPIP_STACK_NetAddress(netH);
            if(ip_address.Val != 0) has_ip = true;                
            SYS_CONSOLE_PRINT("ethernet: IP address changed to %d.%d.%d.%d\r\n",
                    ip_address.v[0],ip_address.v[1],ip_address.v[2],ip_address.v[3]);
            ethernet_services_init();           
        }
        vTaskDelay(333/portTICK_PERIOD_MS);
    }
}

void ethernet_init() {    
    SYS_CONSOLE_PRINT("ethernet: init\r\n");     
        
    (void) xTaskCreate(
        (TaskFunction_t) ethernet_task,
        "ethernet_task",
        1*1024,
        NULL,
        1U ,
        (TaskHandle_t*)NULL);      
}

bool ethernet_hasIP(void){
    return has_ip;
}

bool ethernet_linkUp(void){
    return link_up;
}

void ethernet_tx_packet_ack_callback(TCPIP_MAC_PACKET* pPkt, const void* fParam){
    // Packet transmission completed
    // SYS_CONSOLE_PRINT("ethernet: tx packet ack\r\n");    
    (void)pPkt;
    (void)fParam;    
}

bool ethernet_send(const uint8_t * buf, uint32_t len){
    bool ret = false;
    if(!link_up) return false;
    
    // Get the MAC handle directly from the network interface    
    TCPIP_MAC_HANDLE hMac = TCPIP_STACK_NetToMAC(netH);    
    if(hMac == 0){
        SYS_CONSOLE_PRINT("ethernet: failed to get MAC handle\r\n");
        return false;
    }
    
    // Get the MAC object for packet transmission
    const TCPIP_MAC_OBJECT* pMacObj = TCPIP_STACK_MACObjectGet(netH);
    if(pMacObj == NULL){
        SYS_CONSOLE_PRINT("ethernet: failed to get MAC object\r\n");
        return false;
    }

    // Allocate a proper MAC packet using the stack's allocation function
    TCPIP_MAC_PACKET* pPkt = TCPIP_PKT_SocketAlloc(len, 0, 0, TCPIP_MAC_PKT_FLAG_TX);
    if(pPkt == NULL){
        SYS_CONSOLE_PRINT("ethernet: failed to allocate packet\r\n");
        return false;
    }
    
    // Copy the raw Ethernet frame data
    memcpy(pPkt->pMacLayer, buf, len);
    pPkt->pDSeg->segLen = len;
    pPkt->ackFunc = ethernet_tx_packet_ack_callback;
    pPkt->ackParam = NULL;

    // Send the packet using the MAC object
    TCPIP_MAC_RES result = pMacObj->MAC_PacketTx(hMac, pPkt);
    if(result == TCPIP_MAC_RES_PENDING){
        //SYS_CONSOLE_PRINT("ethernet: tx packet pending\r\n");
        ret = true;
    }
    else if(result == TCPIP_MAC_RES_OK){
        //SYS_CONSOLE_PRINT("ethernet: tx packet sent\r\n");
        ret = true;
    }
    else if(result == TCPIP_MAC_RES_PACKET_ERR || result == TCPIP_MAC_RES_OP_ERR || result == TCPIP_MAC_RES_IS_BUSY){
        SYS_CONSOLE_PRINT("ethernet: tx packet failed: %d\r\n", result);
        // Free the packet if transmission failed immediately
        TCPIP_PKT_PacketFree(pPkt);
        ret = false;
    }
    else{
        SYS_CONSOLE_PRINT("ethernet: tx packet other result: %d\r\n", result);
        ret = false;
    }

    return ret;
}

// New function using proper packet allocation
bool ethernet_send_to(const uint8_t * dstMac, const uint8_t * payload, uint32_t payloadLen, uint16_t etherType) {    
    uint8_t pkt[1518];
    uint8_t * pkt_ptr = pkt;
    memcpy(pkt_ptr, dstMac, 6);
    pkt_ptr += 6;
    memcpy(pkt_ptr, mac_addr, 6);
    pkt_ptr += 6;
    uint16_t type = TCPIP_Helper_htons(etherType);
    memcpy(pkt_ptr, &type, 2);
    pkt_ptr += 2;
    memcpy(pkt_ptr, payload, payloadLen);
    pkt_ptr += payloadLen;
    return ethernet_send(pkt, pkt_ptr - pkt);
}

uint8_t * ethernet_getMACAddress(void){
    return &mac_addr[0];
}

uint8_t * ethernet_getIPAddress(void){
    return ip_address.v;
}