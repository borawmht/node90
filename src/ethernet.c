/*
* Ethernet initialization and management
* ethernet.c
* created by: Brad Oraw
* created on: 2025-08-10
*/

#include "ethernet.h"
#include "definitions.h"
#include "config/default/library/tcpip/src/tcpip_private.h"

TCPIP_NET_HANDLE netH = NULL;

bool has_ip = false;
bool is_ready = false;
bool link_up = false;

uint8_t mac_addr[6] = {0,0,0,0,0,0};
IPV4_ADDR ip_address = {0};

void ethernet_task(void *pvParameters){
    SYS_CONSOLE_PRINT("ethernet: start task\r\n");
    while(1){
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
        }
        vTaskDelay(100/portTICK_PERIOD_MS);
    }
}

void ethernet_init() {    
    SYS_CONSOLE_PRINT("ethernet: init\r\n"); 

    netH = TCPIP_STACK_IndexToNet(0);  
        
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