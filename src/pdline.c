/*
* pdline.c
*/
#include "pdline.h"
#include "resources/event.h"
#include "resources/sensors.h"
#include "definitions.h"

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// Timer3-based timing variables
static volatile uint8_t pf_counter = 0;        // Counts 10us cycles for power/frequency task
static volatile uint32_t com_stage_delay_cycles = 0;

// Timing constants
#define TIMER3_BASE_PERIOD_US    10     // 10 microseconds base period
#define PF_TASK_PERIOD_US        620    // 620 microseconds (62 * 10us cycles)
#define PF_TASK_CYCLES           (PF_TASK_PERIOD_US / TIMER3_BASE_PERIOD_US) 

void pdline_com_stage_task(void);
void pdline_pf_task(void);

// Timer3 interrupt handler - runs every 10us
// void __attribute__((interrupt(IPL5SOFT))) T3_InterruptHandler(void) {
// void __attribute__((interrupt(IPL7SOFT))) T3_InterruptHandler(void) {
// void __attribute__((used)) pdline_timer3_interrupt(void) {
void pdline_task(void) {
    // IFS0bits.T3IF = 0;  // Clear interrupt flag

    // DRV1_LED_Toggle();

    // Just toggle an LED or print something simple
    // static uint32_t counter = 0;
    // counter++;
    // if(counter >= 100000) {  // Every 1-00ms
    //     counter = 0;
    //     SYS_CONSOLE_PRINT("T3 interrupt working!\r\n");
    // }

    // Communication stage task counter
    if(com_stage_delay_cycles > 0) {
        com_stage_delay_cycles--;
        if(com_stage_delay_cycles == 0) {
            pdline_com_stage_task();
        }
    }
    
    // Counter for power/frequency task
    pf_counter++;
    if(pf_counter >= PF_TASK_CYCLES) {
        pf_counter = 0;
        pdline_pf_task();
    }
}

//------------MHTi Communication Constants-----------------------------------------------
#define				SET_DUM						1		// dummy stage at the start of th eprocess
#define				SET_BUSY                    2		// set Busy signal by Node only
#define				SET_NODE                    3		// set Node signal by Node only
#define				SET_PRPH                    4		// set Auxuillary Device signal by AUX device only
#define				SET_LAST                    5		// set or cleared last bit flag indicating last byte if high
#define				SET_BIT						6		// set or cleared data bits by line owner
#define				CLEAR_NODE					7		// clear node signal. By node only

#define				CHECK_BUSY					11		// check Busy signal by peripherals
#define				CHECK_NIDE					12		// check Node signal by Node only
#define				CHECK_PRPH					13		// set Auxuillary Device signal by AUX device only
#define				CHECK_LAST					14		// check to see if this is last byte for RX or TX. It is set by transmitter
#define				CHECK_BIT					15		// set or cleared data bits by line owner
#define				DO_SYNC						16		// bring the SYNC line down
#define				HIGH_SYNC					17		// high portion of second sync clock
#define				LOW_SYNC                    18		// low portion of second clock sync
#define				CLEAR_SYNC					19		// teleas the SYNC line to indicate a sync low to high event


#define				DLY_80us                    8	// ok tested
#define				DLY_160us					16	// ok tested
#define				DLY_220us					22	// ok tested
#define				DLY_280us					28	// ok tested
#define				DLY_380us					38	// ok tested
#define				DLY_520us					52	// ok tested
#define				DLY_600us					60	//
#define				DLY_2400us					240 // 2.4 msec
#define				DLY_2600us					260 // 2.6 msec

#define				MAX_RX_BUF					70
#define				MAX_PRPH_BUF				20				// buffer length for recieving data over prepheral interface

#define             CHECKBIT(var,pos)           ((var) & (1<<(pos)))
#define             SETBIT(var,pos)             ((var) | (1<<(pos)))
#define 		    hibyte(x)                   (uint8_t)(x>>8)
#define 			lobyte(x)                   (uint8_t)(x & 0xFF)

uint8_t tx_buf[MAX_PRPH_BUF]; // tx buffer
uint8_t tx_head = 0; // tx packet head
uint8_t tx_tail = 0; // tx packet tail
uint8_t tx_byte = 0; // data byte being transmitted
uint8_t txb = 0; // bit # being transmitted
uint8_t tx_cs = 0; // check sum of the transmit data
uint8_t framing_cnt = 0;
uint8_t false_byte_cnt = 0;
uint8_t prph_data_buf[MAX_PRPH_BUF]; // prepheral rx buffer
uint8_t prph_pkt_size = 0; // # of bytes in the rx'd periph. packet
uint8_t prph_pkt_vect = 0; // point to the last byte of packet in phph_rx_buf[]
uint8_t prph_rx_buf[MAX_RX_BUF]; // holds the last rx'd packet over peripheral line
uint8_t prph_rx_head = 0; // rx buffer head
uint8_t prph_rx_tail = 0; // rx buffer tail
uint8_t prph_rx_byte = 0; // data byte being recieved
uint8_t rxb = 0; // bit # of the byte being recived
uint8_t com_stage = 0;
uint8_t pf_cnt = 1; //  power/data counter

bool prph_data_ready = false; // when set, it means the rx data is in the buffer
bool reset_requested = false; // flag indicates Node is trying to reset peripherals.
bool rx_flag = false; // flag indicates data rx in progress
bool tx_flag = false; // flag indicates data tx in progress
bool last_rx_flag = false; // flag indicates the last byte is reached in comm.
bool last_tx_flag = false; // flag indicates the last byte of transmission
bool power_flag = false; //	power/data flag
bool check_prph_flag = false;

bool OC1_stat = false;
bool WS1_stat = false;
// uint8_t OCtemp = 72;
// uint16_t active_lumen = 0;

// static uint16_t delay_cnt;

#define A_LONG_TIME 6000000
uint64_t delay_time = A_LONG_TIME; // com stage delay time us

#define				ALL_DEVICES					199		// all devices
#define				WALL_SWITCH					102		//
#define				OCCU_SENSOR					103		//

#define				SET_TAG						101		//
#define				GET_TAG						102		// get tag Added as a cmd
#define				SET_DEV_ID      			103   	//      Set Light Module Serial Number. Must be in special password mode(3 bytes to follow)
#define				CLEAR_TAG					104		// will be calleed by rs232 or UDP line
#define				PREPH_STAT                  105   	//      Get Status bits (one byte)
#define             GET_STATUS					124		//
#define				SET_SENSOR_TYPE             147		// set sensor type
#define             SET_THR						148		// SET Threshold for OC

#define				MOTION_DETECTED			    201		// Occupency sensor detected motion

#define             S1_BUTTON                   205
#define             S2_BUTTON                   206
#define             S3_BUTTON                   207
#define				UP_BUTTON					208		//
#define				DOWN_BUTTON 				209		//
#define				ON_BUTTON	 				210		//
#define				CANCEL_BUTTON	 			211		//

#define             S1_BUTTON_CHECKSUM_BOTH     0x31
#define             S2_BUTTON_CHECKSUM_BOTH     0x32
#define             S3_BUTTON_CHECKSUM_BOTH     0x33
#define             UP_BUTTON_CHECKSUM_BOTH     0x34
#define             DOWN_BUTTON_CHECKSUM_BOTH   0x35
#define             ON_BUTTON_CHECKSUM_BOTH     0x36
#define             CANCEL_BUTTON_CHECKSUM_BOTH 0x37

#define				CLEAR_NODE_TAG				222		// clear tag from ws
#define				RESET_NODE					223		// reset node from WS

#define             TXOn()                      TX_DATA_Set()
#define             TXOff()                     TX_DATA_Clear()
#define             PDCTRLOn()                  PD_CTRL_Set()
#define             PDCTRLOff()                 PD_CTRL_Clear()
#define             RXPINGet()                  RX_DATA_Get()

#define bit_set(r,b) r|(1<<b)
#define bit_test(r,b) CHECKBIT(r, b)
#define load_next_tx_byte() txb=0; \
                            tx_byte=tx_buf[tx_tail]; \
                            tx_tail++; \
                            if(tx_tail>(MAX_PRPH_BUF-1)) tx_tail=0
#define do_set_bit()    if(bit_test(tx_byte, txb)){TXOff();}else{TXOn();} \
                        if(txb<8){ \
                            txb++; \
                            com_stage=SET_BIT; \
                            delay_time=160; \
                        }else{ \
                            TXOff(); \
                            com_stage=0; \
                            if(last_tx_flag){ \
                                tx_flag=false; \
                                last_tx_flag=false; \
                            } \
                            delay_time=A_LONG_TIME; \
                        } \
                        com_stage=com_stage

void create_prep_packet(uint8_t dest, uint8_t cmd, uint8_t upid, uint8_t data_size, uint8_t *data);

static void pdline_setTimer(uint64_t time_us){
    if(time_us < A_LONG_TIME) {
        // Calculate exact number of 10us cycles needed
        com_stage_delay_cycles = (time_us + TIMER3_BASE_PERIOD_US - 1) / TIMER3_BASE_PERIOD_US;
    }
}

void pdline_init(void){
    SYS_CONSOLE_PRINT("pdline: init\r\n");
    
    // Initialize counters
    pf_counter = 0;
    com_stage_delay_cycles = 0;   
}

void go_set_tag(uint8_t tagtype, char * tag){
    // ToDo
}

void go_clear_tag(){
    uint8_t i;
    for(i=1; i<6; i++){
        go_set_tag(i, "0");
    }
}

bool go_get_preph_status(void){
    uint8_t data[1];
    if(!tx_flag){
        data[0]=0;
        create_prep_packet(ALL_DEVICES, GET_STATUS, 22, 1, data); // create the transmision packet
        return true; // If data is being recived, then it will wait
    }
    else return false;
}

bool go_set_prph_tag(uint8_t device_type, uint16_t t, uint8_t device_id){
    uint8_t data[4];
    if(!tx_flag){
        data[0]=device_type; // prepheral type
        data[1]=hibyte(t); // tag high byte
        data[2]=lobyte(t); // tag low byte
        data[3]=device_id; // device number used when there are multiple occ_sensors
        create_prep_packet(device_type, SET_TAG, 0, 4, data); // create the transmision packet
        return true; // If data is being recived, then it will wait
    }
    else return false;
}

void load_bus(uint8_t data){
    tx_buf[tx_head]=data; // This must be first
    tx_cs+=data; // This must be second
    tx_head++; // inc. pointer
    if(tx_head>(MAX_PRPH_BUF-1)) tx_head=0; // limit buffer
}

void create_prep_packet(uint8_t dest, uint8_t cmd, uint8_t upid, uint8_t data_size, uint8_t *data){
    tx_head=0;
    tx_tail=0;
    tx_cs=0; // clear check sum
    load_bus(0x55); // load start byte
    load_bus(dest); // Reciver ID
    load_bus(cmd); // command
    load_bus(upid); // udp_pkt_id if 0 it means no ACK needed
    load_bus(data_size); // Data size
    uint8_t i;
    for(i=0; i<data_size; i++){
        load_bus(data[i]); // load all the data
    }
    load_bus(tx_cs); // load check sum
    tx_flag=true; // set_flag to start transmitting.
}

//---------------------------------------
// Claculate packet check sum and compare
// to the last byte of packet
// if equal return true else return false
//---------------------------------------
bool verify_event_cs(void){
    uint8_t cs=0;
    uint8_t i;
    for(i=0; (i<prph_pkt_size-1); i++) cs+=prph_data_buf[i];
    if(cs==prph_data_buf[prph_pkt_size-1])
        return true;
    else
        return false;
}

void correct_first_byte(void){
    if(prph_data_buf[0]==0x66||
        prph_data_buf[0]==0x77||
        prph_data_buf[0]==0x88||
        prph_data_buf[0]==0xaa||
        prph_data_buf[0]==0x55){
            return; // valid first byte
    }
    uint8_t new_first_bytes[] = {0x66, 0x77, 0x88}; 
    for(uint8_t j = 0; j < 3; j++){
        uint8_t new_first_byte = new_first_bytes[j];
        uint16_t test_checksum = S1_BUTTON_CHECKSUM_BOTH + new_first_byte - 0x66;
        for(uint8_t i=0; i<7; i++){
            if(prph_data_buf[5]==test_checksum){
                prph_data_buf[0] = new_first_byte;
                SYS_CONSOLE_PRINT("pdline: Corrected first byte to 0x%02x\r\n", new_first_byte);
                return;
            }
            test_checksum++;
        }
    }
    if(RXPINGet()){
        SYS_CONSOLE_PRINT("pdline: Failed to correct first byte\r\n");
    }
}

//----------------------------------------------------
// Data from peripheral is ready, so read the buffer
//----------------------------------------------------
void go_check_prph(void){
    uint8_t ack_pkt_id;
    uint8_t client;
    uint8_t cmd;
    uint8_t i;
    char msg[128];
    char * event_name = "none";
    check_prph_flag=true;
    correct_first_byte();
    if (prph_data_buf[0]==0xaa){
        // SYS_CONSOLE_PRINT("PDLine Ack\r\n");
    }
    else if(prph_data_buf[0]==0x55){
        cmd=prph_data_buf[6];
        // process data packet
        //if(verify_event_cs()){
        if(true){
            framing_cnt=0;
            switch(cmd){
                case PREPH_STAT:
                    client=prph_data_buf[3];
                    switch(client){
                        case WALL_SWITCH:
                            WS1_stat=true;
                            break;
                    }
                    break;
            }
        }
        else{
            //SYS_CONSOLE_PRINT(" Prph Fail\n\r");
        }
    }
    else if(prph_data_buf[0]==0x66||
            prph_data_buf[0]==0x77||
            prph_data_buf[0]==0x88){
        // debug print event packet
        // SYS_CONSOLE_PRINT("PDLine Packet: ");
        // for(i=0; (i<prph_pkt_size); i++) SYS_CONSOLE_PRINT("%02x [%d], ", prph_data_buf[i], prph_data_buf[i]);
        // SYS_CONSOLE_PRINT("\n\r");
        // end debug print event packet
        // process event packet
        SYS_CONSOLE_PRINT("pdline: WallSwitch ");
        // if(verify_event_cs()){
        if(true){
            if(prph_data_buf[0]==0x66){
                // driver1_only_flag=true;
                // driver2_only_flag=true;
                SYS_CONSOLE_PRINT("both channels ");
            }
            else if(prph_data_buf[0]==0x77){//driver 1 only
                // driver1_only_flag=true;
                // driver2_only_flag=false;
                SYS_CONSOLE_PRINT("driver1 only ");
            }
            else{//driver 2 only
                // driver1_only_flag=false;
                // driver2_only_flag=true;
                SYS_CONSOLE_PRINT("driver2 only ");
            }
            framing_cnt=0;
            if(prph_data_buf[0]==0x66&&prph_data_buf[3]==MOTION_DETECTED){
                // OC1_stat=true;
                // prphCluster=getKeyWordFromSensor(gDefaultSensorStr);
                // eventID=EV_MOTION;
            }
            else if(prph_data_buf[3]!=MOTION_DETECTED){
                WS1_stat=true;
                SYS_CONSOLE_PRINT("Command 0x%02x [%d] ", prph_data_buf[3], prph_data_buf[3]);
                if(prph_data_buf[3]==CLEAR_NODE_TAG) go_clear_tag();
                //else if(prph_data_buf[3]==RESET_NODE) esp_restart();
                else{
                    switch(prph_data_buf[3]){
                        case ON_BUTTON:
                            event_name = "on";
                            break;
                        case CANCEL_BUTTON:
                            event_name = "off";
                            break;
                        case UP_BUTTON:
                            event_name = "up";
                            break;
                        case DOWN_BUTTON:
                            event_name = "down";
                            break;
                        case S3_BUTTON:
                            event_name = "s3";
                            break;
                        case S2_BUTTON:
                            event_name = "s2";
                            break;
                        case S1_BUTTON:
                            event_name = "s1";
                            break;

                        default:
                            event_name = "none";
                            break;
                    }
                    if(strcmp(event_name, "none") == 0){
                        // error correction, use checksum to determine the correct event name
                        uint16_t checksum = prph_data_buf[5] + prph_data_buf[0] - 0x66;
                        switch(checksum){
                            case S3_BUTTON_CHECKSUM_BOTH:
                                event_name = "s3";
                                break;
                            case S2_BUTTON_CHECKSUM_BOTH:
                                event_name = "s2";
                                break;
                            case S1_BUTTON_CHECKSUM_BOTH:
                                event_name = "s1";
                                break;
                            case UP_BUTTON_CHECKSUM_BOTH:
                                event_name = "up";
                                break;
                            case DOWN_BUTTON_CHECKSUM_BOTH:
                                event_name = "down";
                                break;
                            case ON_BUTTON_CHECKSUM_BOTH:
                                event_name = "on";
                                break;
                            case CANCEL_BUTTON_CHECKSUM_BOTH:
                                event_name = "off";
                                break;
                            default:
                                event_name = "none";
                                break;
                        }
                    }
                    event_send_key_value(ethernet_getBroadcastAddressString(),
                        event_name,sensors_wallswitch_get_cluster(0),true);
                    event_execute_key_value(event_name,sensors_wallswitch_get_cluster(0));
                }
            }
        }
        else{
            // log bad event link
            SYS_CONSOLE_PRINT("Failed Event: ");
            for(i=0; (i<prph_pkt_size); i++) SYS_CONSOLE_PRINT("%02x, ", prph_data_buf[i]);
            SYS_CONSOLE_PRINT("\n\r");
        }        
    }
    else{
        // log unknown rx error
        // SYS_CONSOLE_PRINT("Prph Framing Error Carry on!\r\n");
        // framing_cnt++;
        // if(framing_cnt>100) esp_restart();
        if(RXPINGet()){
            SYS_CONSOLE_PRINT("pdline: Framing Error: ");
            for(i=0; (i<prph_pkt_size); i++) SYS_CONSOLE_PRINT("%02x, ", prph_data_buf[i]);
            SYS_CONSOLE_PRINT("\n\r");
        }
    }
    check_prph_flag=false;
}

// 10ms task
int16_t pdline_i = 0;
void pdline_data_ready_task(void){
    if(prph_data_ready){
        prph_data_ready=false;
        pdline_i=0;
        while(prph_rx_tail!=prph_pkt_vect){
            prph_data_buf[pdline_i]=prph_rx_buf[prph_rx_tail]; // xfer data to packet holder from rotory buffer
            // SYS_CONSOLE_PRINT("prph_data_buf[%d]=%02x\r\n", pdline_i, prph_data_buf[pdline_i]); // debug
            pdline_i++;
            if(pdline_i>(MAX_PRPH_BUF-1)){
                prph_rx_tail=0;
                prph_rx_head=0;
                break;
            }
            prph_rx_tail++;
            if(prph_rx_tail>(MAX_RX_BUF-1)) prph_rx_tail=0;
        }
        prph_pkt_size=pdline_i; //
        go_check_prph(); // check for data from prepheral
        false_byte_cnt=0;
    }
}

void pdline_com_stage_task(void){ // 10us task
    delay_time = 80; // us
    switch(com_stage){
        case SET_BUSY:
            delay_time = 160; // us
            if(rx_flag||tx_flag) // if RX in progress then lock the line
                TXOn();
            else // else release the line
                TXOff();
            com_stage=SET_NODE; // set stage for tx or nothing
            break;

        case SET_NODE: // busy signal
            if(rx_flag){ // check tx in progress
                delay_time = 520; // wake up in 520 usec to check if last rx byte 160+280+80
                TXOff(); // release the line for peripheral to take over
                com_stage=CHECK_LAST; // set stage for rx process
            }
            else if(tx_flag){
                delay_time = 160; // then continue with tx process
                TXOn();
                com_stage=CLEAR_NODE; // go to stage to release line to indicated end of NODE bit
            }
            else if(reset_requested){
                reset_requested=false; // honor the request
                TXOn();
                delay_time = 600; // wait 600 uSec and then
                com_stage=HIGH_SYNC; // send SYNC signal 160+280+160
            }
            else{			
                TXOff(); // otherwise release the line indicating no master activity
                delay_time = 380; // wait 380 uSec and then look for possible rx process to start
                com_stage=CHECK_PRPH; // set stage to Check Peripheral 160+220
            }
            break;

        case CLEAR_NODE:
            delay_time = 280; // then continue with tx process  120 + 280
            TXOff(); // lock the line Node activities
            com_stage=SET_LAST; // goto SET_LAST and release line if last byte
            break;

        case CHECK_PRPH:
            delay_time = 220; // us
            com_stage=HIGH_SYNC; // send SYNC signal anyway
            if(!RXPINGet()){ // Check to see if peripherals have pulled the line down
                rx_flag=true; // declare rx process started
                rxb=0; // set rx bit position to bit 0
                prph_rx_byte=0; // clear rx byte
            }
            break;

        case SET_LAST:
            delay_time = 160; // us
            load_next_tx_byte();
            if(tx_tail==tx_head){
                TXOn(); // pull line down to indicate last byte
                last_tx_flag=true;
            }
            else{
                TXOff(); // release line to indicate not last byte
            }
            com_stage=HIGH_SYNC; // proceed to SET SYNC bit
            break;

        case CHECK_LAST:
            delay_time = 80; // us
            if(!RXPINGet()) last_rx_flag=true;
            else last_rx_flag=false;
            com_stage=HIGH_SYNC;
            break;

        case HIGH_SYNC:
            TXOff(); // pull line down to create a SYNC signal
            delay_time = 80; // wait one bit duration
            com_stage=LOW_SYNC; //
            break;

        case LOW_SYNC:
            TXOn(); // pull line down to create a SYNC signal
            delay_time = 80; // wait one bit duration
            com_stage=CLEAR_SYNC; //
            break;

        case CLEAR_SYNC:
            TXOff(); // release line to indicate a rising edge to SYNC
            if(rx_flag){
                delay_time = 80; // set stage to send SYNC for peripheral
                com_stage=CHECK_BIT; // data rx stage
            }
            else if(tx_flag){
                do_set_bit();
            }
            else
                delay_time = A_LONG_TIME; // stop and wait for pf task to restart
            break;

        case CHECK_BIT:
            delay_time = 160; // RX in progress. Set timer for to mid bit 7 of the next byte in 155 msec
            if(RXPINGet()){ // if line is high then data bit is 1
                prph_rx_byte=bit_set(prph_rx_byte, rxb); // so set the appropriate bit in rx_byte
            }

            rxb++;
            if(rxb>7){
                prph_rx_buf[prph_rx_head]=prph_rx_byte; // end of rx process for the present byte
                prph_rx_head++; // inc. rx head
                if(prph_rx_head>(MAX_RX_BUF-1)) prph_rx_head=0;
                if(prph_rx_byte==0xFF){
                    false_byte_cnt++;
                    if(false_byte_cnt>20){
                        rx_flag=false;
                    }
                }
                prph_rx_byte=0;
                rxb=0;
                delay_time = A_LONG_TIME; // stop and wait for pf task to restart
                com_stage=0;

                if(last_rx_flag){
                    last_rx_flag=false;
                    prph_data_ready=true; // declare data byte has been recieved
                    rx_flag=false;
                    prph_pkt_vect=prph_rx_head;
                }

            }
            break;

        case SET_BIT: // busy signal
            do_set_bit();
            break;

        default: com_stage=0;
            break;
    }
    if(delay_time<A_LONG_TIME)
        pdline_setTimer(delay_time);
}

void pdline_pf_task(void){ // 617us task
    pf_cnt --;
    if(pf_cnt==0){
        power_flag = !power_flag;
        if(power_flag){
            pf_cnt = 4; //
            PDCTRLOn(); // Turn power on for peripherals
            TXOff();    // deliver power to peripheral sevice
        }
        else {
            pf_cnt = 3;
            PDCTRLOff();
            pdline_setTimer(80); // us, one bit duration 
            com_stage=SET_BUSY;
            TXOn();		
        }     
    }
}