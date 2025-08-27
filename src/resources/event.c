/*
* event.c
* created by: Brad Oraw
* created on: 2025-08-27
*/

#include "event.h"
#include "policy.h"
#include "control.h"
#include "resources/actuators.h"
#include "resources.h"
#include "ethernet.h"
#include "cJSON.h"
#include "jsoncbor.h"
#include "cborjson.h"
#include "definitions.h"

bool event_get_cluster_match(char * event_cluster, char * cluster_to_match){
    char *broadcast_cluster="0";
    if(strcmp(event_cluster, cluster_to_match)==0 ||
           strcmp(event_cluster, broadcast_cluster)==0){
        return true;
    }
    bool match = false;
    char * tok;
    char * zone_delimiter = ".";
    char event_cluster_cpy[64];
    char cluster_to_match_cpy[64];
    strcpy(&event_cluster_cpy[0],event_cluster); // make a local copy to parse
    strcpy(&cluster_to_match_cpy[0],cluster_to_match); // make a local copy to parse
    char event_cluster_id[64];
    event_cluster_id[0] = 0; // empty string
    char event_zone[64];
    event_zone[0] = 0; // empty string
    tok = strtok(&event_cluster_cpy[0],zone_delimiter); // split between delimiters
    if(tok!=NULL){
        strcpy(&event_cluster_id[0],tok); // copy id
        //SYS_CONSOLE_PRINT("event_cluster_id %s\r\n",&event_cluster_id[0]);
    }
    tok = strtok(NULL,zone_delimiter); // split between delimiters
    if(tok!=NULL){
        strcpy(&event_zone[0],tok); // copy zone
        //SYS_CONSOLE_PRINT("event_zone %s\r\n",&event_zone[0]);
    }
    tok = strtok(&cluster_to_match_cpy[0],zone_delimiter); // split between delimiters
    if(tok!=NULL){
        if(strcmp(&event_cluster_id[0], tok)==0 ||
           strcmp(&event_cluster_id[0], broadcast_cluster)==0){
            //SYS_CONSOLE_PRINT("actuator_cluster_id %s\r\n",tok);
            match = true; // match on cluster id
        }
        tok = strtok(NULL,zone_delimiter); // split between delimiters
    }
    if(match && event_zone[0]!=0){
        match = false; // clear match
        while(tok!=NULL){
            //SYS_CONSOLE_PRINT("actuator_zone %s\r\n",tok);
            if(strcmp(&event_zone[0], tok)==0){
                match = true; // match on zone
                break;
            }
            tok = strtok(NULL,zone_delimiter); // split between delimiters
        }
    }
    return match;
}

enum{COLOR_TYPE_NONE,COLOR_TYPE_AT,COLOR_TYPE_RGBW};
char policy_cpy[POLICY_STR_SIZE];
bool event_execute_policy_command(char * policy_command, char * cluster){
    strcpy(&policy_cpy[0],policy_command);
    SYS_CONSOLE_PRINT("event: execute policy command: %s\r\n",&policy_cpy[0]);
    //return true;
    char * tok;
    tok = strtok(&policy_cpy[0],",;");
    uint8_t i;
    uint16_t channel;
    uint16_t value;
    uint16_t dim = DIM_NO_CHANGE;
    uint16_t dim_default = DIM_NO_CHANGE;
    int32_t dim_duration = -1;
    uint8_t color_type, color_group;
    uint16_t color_values[4];
    uint32_t rgbw_value;
    uint16_t at_value;
    while(tok!=NULL){
        if(strcmp(tok,"F0")==0){ // set dim
            dim = DIM_NO_CHANGE;
            dim_duration = -1;
            dim_default = DIM_NO_CHANGE;
            tok = strtok(NULL,",;");
            if(tok!=NULL) channel = atoi(tok);
            else break;
            tok = strtok(NULL,",;");
            if(tok!=NULL) dim = (uint16_t)atoi(tok);
            else break;
            for(i=0;i<NUM_ACTUATORS;i++){
                bool match = event_get_cluster_match(cluster, actuators_actuator_get_cluster(i+1));
                if(match && (channel==i+1 || channel==0)){
                    if(dim<DIM_NO_CHANGE) control_setDimValue(i+1,dim);
                    control_setDimDuration(i+1,dim_duration,dim_default); // cancel any existing dim duration                    
                }
            }
        }
        else if(strcmp(tok,"F1")==0){ // set dim duration
            dim = DIM_NO_CHANGE;
            dim_duration = -1;
            dim_default = DIM_NO_CHANGE;
            tok = strtok(NULL,",;");
            if(tok!=NULL) channel = (uint16_t)atoi(tok);
            else break;
            tok = strtok(NULL,",;");
            if(tok!=NULL) dim = (uint16_t)atoi(tok);
            else break;
            tok = strtok(NULL,",;");
            if(tok!=NULL) dim_duration = atoi(tok);
            tok = strtok(NULL,",;");
            if(tok!=NULL) dim_default = (uint16_t)atoi(tok);
            for(i=0;i<NUM_ACTUATORS;i++){
                bool match = event_get_cluster_match(cluster, actuators_actuator_get_cluster(i+1));
                if(match && (channel==i+1 || channel==0)){
                    if(dim<DIM_NO_CHANGE) control_setDimValue(i+1,dim);
                    control_setDimDuration(i+1,dim_duration,dim_default); // cancel any existing dim duration
                }
            }
        }
        else if(strcmp(tok,"F2")==0){ // set dim up
            dim = DIM_NO_CHANGE;
            dim_duration = -1;
            dim_default = DIM_NO_CHANGE;
            tok = strtok(NULL,",;");
            if(tok!=NULL) channel = atoi(tok);
            else break;
            tok = strtok(NULL,",;");
            if(tok!=NULL) value = atoi(tok);
            else break;
            for(i=0;i<NUM_ACTUATORS;i++){
                bool match = event_get_cluster_match(cluster, actuators_actuator_get_cluster(i+1));
                if(match && (channel==i+1 || channel==0)){
                    if(DIM_MAX>=(control_getDimValue(i+1)+value)){
                        control_setDimValue(i+1,control_getDimValue(i+1)+value);
                    }
                    else{
                        control_setDimValue(i+1,DIM_MAX);
                    }
                    control_setDimDuration(i+1,dim_duration,dim_default); // cancel any existing dim duration
                }
            }
        }
        else if(strcmp(tok,"F3")==0){ // set dim down
            dim = DIM_NO_CHANGE;
            dim_duration = -1;
            dim_default = DIM_NO_CHANGE;
            tok = strtok(NULL,",;");
            if(tok!=NULL) channel = atoi(tok);
            else break;
            tok = strtok(NULL,",;");
            if(tok!=NULL) value = atoi(tok);
            else break;
            for(i=0;i<NUM_ACTUATORS;i++){
                bool match = event_get_cluster_match(cluster, actuators_actuator_get_cluster(i+1));
                if(match && (channel==i+1 || channel==0)){
                    if(control_getDimValue(i+1)>=(DIM_MIN+value)){
                        control_setDimValue(i+1,control_getDimValue(i+1)-value);
                    }
                    else{
                        control_setDimValue(i+1,DIM_MIN);
                    }
                    control_setDimDuration(i+1,dim_duration,dim_default); // cancel any existing dim duration
                }
            }
        }
        else if(strcmp(tok,"F9")==0){ // set color values
            bool match = true;
            for(i=0;i<NUM_ACTUATORS;i++){
                match &= event_get_cluster_match(cluster, actuators_actuator_get_cluster(i+1));
            }
            if(!match) break;
            color_type = COLOR_TYPE_NONE;
            at_value = COLOR_NO_CHANGE;
            color_values[0] = COLOR_NO_CHANGE;
            color_values[1] = COLOR_NO_CHANGE;
            color_values[2] = COLOR_NO_CHANGE;
            color_values[3] = COLOR_NO_CHANGE;
            tok = strtok(NULL,",;");
            if(strcmp(tok,"AT")==0) color_type = COLOR_TYPE_AT;
            else if(strcmp(tok,"RGBW")==0) color_type = COLOR_TYPE_RGBW;
            else break;
            tok = strtok(NULL,",;");
            if(tok!=NULL) color_group = (uint16_t)atoi(tok);
            else break;
            tok = strtok(NULL,",;");
            if((tok!=NULL)&&(color_type==COLOR_TYPE_AT)) at_value = (uint16_t)atoi(tok);
            else if((tok!=NULL)&&(color_type==COLOR_TYPE_RGBW)) color_values[0] = (uint16_t)atoi(tok);
            else break;
            if(color_type==COLOR_TYPE_RGBW){
                // if(color_values[0]>=COLOR_NO_CHANGE) break;
                // rgbw_value = ((uint32_t)color_values[0])<<24;
                // for(i=1;i<4;i++){
                //     tok = strtok(NULL,",;");
                //     if(tok!=NULL) color_values[i] = (uint16_t)atoi(tok);
                //     else break;
                //     if(color_values[i]>=COLOR_NO_CHANGE) break;
                //     rgbw_value += ((uint32_t)color_values[i])<<((3-i)*8);
                // }
                // control_setRGBWValue(rgbw_value);
            }
            else{                
                if((at_value<AT_MIN)||(at_value>AT_MAX)) break;
                control_setATValue(at_value);
            }
        }
        else if(strcmp(tok,"F10")==0){ // at color up
            bool match = true;
            for(i=0;i<NUM_ACTUATORS;i++){
                match &= event_get_cluster_match(cluster, actuators_actuator_get_cluster(i+1));
            }
            if(!match) break;
            tok = strtok(NULL,",;");
            if(tok!=NULL) color_group = (uint16_t)atoi(tok);
            else break;
            tok = strtok(NULL,",;");
            if(tok!=NULL) value = atoi(tok);
            else break;
            if(AT_MAX>=(control_getATValue()+value)){
                control_setATValue(control_getATValue()+value);
            }
            else{
                control_setATValue(AT_MAX);
            }
        }
        else if(strcmp(tok,"F11")==0){ // at color down
            bool match = true;
            for(i=0;i<NUM_ACTUATORS;i++){
                match &= event_get_cluster_match(cluster, actuators_actuator_get_cluster(i+1));
            }
            if(!match) break;
            tok = strtok(NULL,",;");
            if(tok!=NULL) color_group = (uint16_t)atoi(tok);
            else break;
            tok = strtok(NULL,",;");
            if(tok!=NULL) value = atoi(tok);
            else break;
            if(control_getATValue()>=(AT_MIN+value)){
                control_setATValue(control_getATValue()-value);
            }
            else{
                control_setATValue(AT_MIN);
            }
        }
        tok = strtok(NULL,",;");
    }
    return true;
}

bool event_execute_policy_name(char * policy_name, char * cluster){
    if(strcmp(policy_name,"on")==0){
        event_execute_policy_command(policy_get_on(1),cluster);
    }
    else if(strcmp(policy_name,"off")==0){
        event_execute_policy_command(policy_get_off(1),cluster);
    }
    else if(strcmp(policy_name,"up")==0){
        event_execute_policy_command(policy_get_up(1),cluster);
    }
    else if(strcmp(policy_name,"down")==0){
        event_execute_policy_command(policy_get_down(1),cluster);
    }
    else if(strcmp(policy_name,"mot")==0){
        event_execute_policy_command(policy_get_mot(1),cluster);
    }
    else if(strcmp(policy_name,"vac")==0){
        event_execute_policy_command(policy_get_vac(1),cluster);
    }
    else if(strcmp(policy_name,"s1")==0){
        event_execute_policy_command(policy_get_s1(1),cluster);
    }
    else if(strcmp(policy_name,"s2")==0){
        event_execute_policy_command(policy_get_s2(1),cluster);
    }
    else if(strcmp(policy_name,"s3")==0){
        event_execute_policy_command(policy_get_s3(1),cluster);
    }
    else{
        SYS_CONSOLE_PRINT("event: execute policy name: %s not found\r\n", policy_name);
    }
    return true;
}

bool event_execute_special(const char * key, const char * value){
    char value_cpy[POLICY_STR_SIZE];
    strcpy(&value_cpy[0],value); // make a copy to parse
    char * tok = strtok(&value_cpy[0],","); // cluster
    char cluster[64];
    strcpy(&cluster[0],tok); // make a copy
    tok = strtok(NULL,","); // first parameter
    uint8_t i;
    uint16_t channel = 0;
    uint16_t dim = DIM_NO_CHANGE;
    int32_t dim_duration = -1;
    uint16_t dim_default = DIM_NO_CHANGE;
    uint16_t at_value = COLOR_NO_CHANGE;
    uint32_t rgbw_value = 0;
    uint16_t rgbw_values[4] = {COLOR_NO_CHANGE,COLOR_NO_CHANGE,COLOR_NO_CHANGE,COLOR_NO_CHANGE};
    if(strcmp(key,"fl")==0){
        for(i=0; i<3; i++){
            if(tok==NULL) break;
            if(i==0) channel = (uint16_t)atoi(tok);
            if(i==1) dim = (uint16_t)atoi(tok);
            if(i==2) dim_duration = atoi(tok);
            if(i==3) dim_default = (uint16_t)atoi(tok);
            tok = strtok(NULL,","); // next parameter
        }
        for(i=0;i<NUM_ACTUATORS;i++){
            bool match = event_get_cluster_match(cluster, actuators_actuator_get_cluster(i+1));
            if(match && (channel==i+1 || channel==0)){
                if(dim < DIM_NO_CHANGE){
                    control_setDimValue(i+1,dim);
                }
                if((dim_default < DIM_NO_CHANGE)&&(dim_duration>=0)){
                    control_setDimDuration(i+1,dim_duration,dim_default);
                }
            }
        }
    }
    else if(strcmp(key,"attune")==0){
        bool match = true;
        bool autotune = true;
        for(i=0;i<NUM_ACTUATORS;i++){
            match &= event_get_cluster_match(cluster, actuators_actuator_get_cluster(i+1));
            autotune &= (strncmp(actuators_actuator_get_pwm_mode(i+1),"AT",2)==0);
        }
        if(!match || !autotune) return false;
        for(i=0; i<2; i++){
            if(tok==NULL) break;
            if(i==0) at_value = (uint16_t)atoi(tok);
            if(i==1) dim = (uint16_t)atoi(tok);
            tok = strtok(NULL,","); // next parameter
        }
        if(dim < DIM_NO_CHANGE){
            control_setDimValue(0,dim);
            control_setDimValue(1,dim);
        }
        if((at_value>=AT_MIN)&&(at_value<=AT_MAX)){
            control_setATValue(at_value);
        }
    }
    // else if(strcmp(key,"rgbw")==0){
    //     for(i=0; i<5; i++){
    //         if(tok==NULL) break;
    //         if(i==4) dim = (uint16_t)atoi(tok);
    //         else rgbw_values[i] = (uint16_t)atoi(tok);
    //         tok = strtok(NULL,","); // next parameter
    //     }
    //     if(dim < DIM_NO_CHANGE){
    //         control_setDimValue(dim);
    //     }
    //     bool color_changed = true;
    //     rgbw_value = 0;
    //     for(i=0; i<4; i++){
    //         color_changed &= (rgbw_values[i]<COLOR_NO_CHANGE);
    //         if(!color_changed) break;
    //         rgbw_value += ((uint32_t)rgbw_values[i])<<((3-i)*8); 
    //     }
    //     if(color_changed){
    //         control_setRGBWValue(rgbw_value);
    //     }
    // }
    return true;
}

void event_init(void){
    SYS_CONSOLE_PRINT("event: init\r\n");
}

bool event_execute(char * key, char * value){
    // SYS_CONSOLE_PRINT("event: execute: %s: %s\r\n", key, value);
    char msg[516];
    char special[128];
    char * cluster = &value[0];
    bool special_event = false;
    if((strcmp(key,"fl")==0)||(strcmp(key,"attune")==0)||(strcmp(key,"rgbw")==0)){
        //printf("special event\r\n");
        strcpy(&special[0],&value[0]); // make a copy to parse
        cluster = strtok(&special[0],",");
        special_event = true;
    }        
    bool match = false;
    for(int i=0;i<NUM_ACTUATORS;i++){
        if(event_get_cluster_match(cluster, actuators_actuator_get_cluster(i+1))){
            match = true;
            printf("event: cluster match: %s, actuator%d\r\n",cluster,i+1);            
        }
    }
    if(match){
        SYS_CONSOLE_PRINT("event: cluster match, execute: %s: %s\r\n", key, value);
    }
    if(!match){
        // SYS_CONSOLE_PRINT("cluster no match\r\n");
    }
    else if(special_event){
        event_execute_special(key, value);
    }
    else if(key[0]=='F'){
        event_execute_policy_command(key, value);
    }
    else{
        event_execute_policy_name(key, value);
    }
    //shades_handleEvent(key, value);
    return true;
}

char * event_get_json_str(void){
    sprintf(resource_json_str, "{\"key\": \"%s\"}", "value");
    return resource_json_str;
}

bool event_coap_get_handler(coap_message_t *response){
    SYS_CONSOLE_PRINT("event: event coap get handler\r\n");
    snprintf(resource_e_json_str,RESOURCE_E_JSON_STR_SIZE,"{\"e\":%s}",event_get_json_str());
    size_t encoded_size = 0;
    CborError error = json_to_cbor(resource_e_json_str, resource_cbor_buffer, RESOURCE_CBOR_BUFFER_SIZE, &encoded_size);
    if (error != CborNoError) {
        SYS_CONSOLE_PRINT("event: json_to_cbor error: %d\r\n", error);
        return false;      
    }
    
    response->code = COAP_CODE_CONTENT;
    response->content_format = COAP_CONTENT_FORMAT_APPLICATION_CBOR;
    response->payload_length = encoded_size;
    memcpy(response->payload, resource_cbor_buffer, response->payload_length);

    // SYS_CONSOLE_PRINT("event: response: %s\r\n", resource_e_json_str);
    // break long print into parts, split on characters
    // SYS_CONSOLE_PRINT("event: response: ");
    // int len = strlen(resource_e_json_str);
    // for(int i = 0; i < len; i++){
    //     SYS_CONSOLE_PRINT("%c", resource_e_json_str[i]);
    // }    
    // SYS_CONSOLE_PRINT("\r\n");
    return true;
}

bool event_put_json_str(char * json_str){
    // SYS_CONSOLE_PRINT("event: event put json str: %s\r\n", json_str);
    // parse json
    bool ret = true;
    bool found = false;
    cJSON * root = cJSON_Parse(json_str);
    cJSON * map = root;
    cJSON * e = cJSON_GetObjectItem(root,"e");    
    if(e){
        map = e;
    }
    
    // Get the first item from the object
    if(map && map->child){
        cJSON * first_item = map->child;
        if(first_item->string && first_item->valuestring){
            ret &= event_execute(first_item->string, first_item->valuestring);
            found = true;
        }
    }
    
    ret &= found;
    cJSON_Delete(root);
    return ret;
}    

bool event_coap_put_handler(const coap_message_t *request, coap_message_t *response){
    // SYS_CONSOLE_PRINT("event: event coap put handler\r\n");
    // cbor to json
    size_t encoded_size = request->payload_length;
    cbor_to_json_string(request->payload, encoded_size, resource_json_str, RESOURCE_JSON_STR_SIZE, &encoded_size, 0);        
    return event_put_json_str(resource_json_str);
}

bool event_coap_handler(const coap_message_t *request, coap_message_t *response) {
    if(request->code == COAP_CODE_GET){
        return event_coap_get_handler(response);
    }
    else if(request->code == COAP_CODE_PUT){
        return event_coap_put_handler(request, response);
    }
    return false;
}
