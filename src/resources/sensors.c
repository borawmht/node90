/*
* Sensors Resource
* sensors.c
* created by: Brad Oraw
* created on: 2025-08-27
*/

#include "sensors.h"
#include "resources.h"
#include "sense.h"
#include "ethernet.h"
#include "cJSON.h"
#include "jsoncbor.h"
#include "cborjson.h"
#include "definitions.h"
#include "storage.h"

sensor_t sensors[NUM_SENSORS];
wallswitch_t wallswitches[NUM_WALLSWITCHES];

void sensors_init(void){
    SYS_CONSOLE_PRINT("sensors: init\r\n");
    for(int i = 0; i < NUM_SENSORS; i++){
        sensors[i].channel = i+1;
        sprintf(sensors[i].ns, "sensor%d", i+1);
        storage_loadStrIndex(sensors[i].ns, "cluster", sensors[i].cluster, "group1", i+1, &sensors_sensor_set_cluster); 
        storage_loadStrIndex(sensors[i].ns, "prphtag", sensors[i].prphtag, "0", i+1, &sensors_sensor_set_prphtag); 
        storage_loadStrIndex(sensors[i].ns, "type", sensors[i].type, "INPUT_LH", i+1, &sensors_sensor_set_type); 
        storage_loadStrIndex(sensors[i].ns, "eventlh", sensors[i].eventlh, "mot", i+1, &sensors_sensor_set_eventlh); 
        storage_loadStrIndex(sensors[i].ns, "eventhl", sensors[i].eventhl, "vac", i+1, &sensors_sensor_set_eventhl); 
        storage_loadU16Index(sensors[i].ns, "holdtime", &sensors[i].holdtime, 0, i+1, &sensors_sensor_set_holdtime); 
        storage_loadU16Index(sensors[i].ns, "occupiedtimeout", &sensors[i].occupiedtimeout, 0, i+1, &sensors_sensor_set_occupiedtimeout); 
        storage_loadU16Index(sensors[i].ns, "vaccanttimeout", &sensors[i].vaccanttimeout, 60, i+1, &sensors_sensor_set_vaccanttimeout); 
        storage_loadU16Index(sensors[i].ns, "high_threshold", &sensors[i].high_threshold, 2500, i+1, &sensors_sensor_set_high_threshold); 
        storage_loadU16Index(sensors[i].ns, "low_threshold", &sensors[i].low_threshold, 2000, i+1, &sensors_sensor_set_low_threshold); 
    }   
    for(int i = 0; i < NUM_WALLSWITCHES; i++){
        wallswitches[i].channel = i+1;
        sprintf(wallswitches[i].ns, "wallswitch%d", i+1);
        storage_loadStrIndex(wallswitches[i].ns, "cluster", wallswitches[i].cluster, "group1", i+1, &sensors_wallswitch_set_cluster); 
        storage_loadStrIndex(wallswitches[i].ns, "prphtag", wallswitches[i].prphtag, "0", i+1, &sensors_wallswitch_set_prphtag); 
    }
}

char * sensors_get_json_str(void) {    
    cJSON * root = cJSON_CreateObject();
    cJSON * sensors_array = cJSON_CreateArray();    
    for(int i = 0; i < NUM_SENSORS; i++){
        cJSON * sensor = cJSON_CreateObject();
        cJSON_AddNumberToObject(sensor,"channel",sensors[i].channel);
        char uri[64];
        sprintf(uri,"inx/sensors/sensor%u",i+1);
        cJSON_AddStringToObject(sensor,"uri",uri);
        cJSON_AddItemToArray(sensors_array, sensor);
    }    
    cJSON_AddItemToObject(root, "sensors", sensors_array);
    cJSON * wallswitches_array = cJSON_CreateArray();    
    for(int i = 0; i < NUM_WALLSWITCHES; i++){
        cJSON * wallswitch = cJSON_CreateObject();
        cJSON_AddNumberToObject(wallswitch,"channel",wallswitches[i].channel);
        char uri[64];
        //sprintf(uri,"inx/sensors/WallSwitch%u",i+1);
        sprintf(uri,"inx/sensors/WallSwitch");
        cJSON_AddStringToObject(wallswitch,"uri",uri);
        cJSON_AddItemToArray(wallswitches_array, wallswitch);
    }    
    cJSON_AddItemToObject(root, "wallswitches", wallswitches_array);
    char * print_str = cJSON_PrintUnformatted(root);
    strncpy(resource_json_str,print_str,RESOURCE_JSON_STR_SIZE);
    cJSON_free(print_str);
    cJSON_Delete(root);
    return resource_json_str;
}

bool sensors_coap_get_handler(coap_message_t *response){
    SYS_CONSOLE_PRINT("sensors: coap get handler\r\n");
    snprintf(resource_e_json_str,RESOURCE_E_JSON_STR_SIZE,"{\"e\":%s}",sensors_get_json_str());
    size_t encoded_size = 0;
    CborError error = json_to_cbor(resource_e_json_str, resource_cbor_buffer, RESOURCE_CBOR_BUFFER_SIZE, &encoded_size);
    if (error != CborNoError) {
        SYS_CONSOLE_PRINT("sensors: json_to_cbor error: %d\r\n", error);
        return false;      
    }
    
    response->code = COAP_CODE_CONTENT;
    response->content_format = COAP_CONTENT_FORMAT_APPLICATION_CBOR;
    response->payload_length = encoded_size;
    memcpy(response->payload, resource_cbor_buffer, response->payload_length);

    // SYS_CONSOLE_PRINT("sensors: response: %s\r\n", resource_e_json_str);
    // break long print into parts, split on characters
    SYS_CONSOLE_PRINT("sensors: response: ");
    int len = strlen(resource_e_json_str);
    for(int i = 0; i < len; i++){
        SYS_CONSOLE_PRINT("%c", resource_e_json_str[i]);
    }    
    SYS_CONSOLE_PRINT("\r\n");
    return true;
}

bool sensors_put_json_str(char * json_str){
    SYS_CONSOLE_PRINT("sensors: put json str: %s\r\n", json_str);
    return false;
}

bool sensors_coap_put_handler(const coap_message_t *request, coap_message_t *response){
    SYS_CONSOLE_PRINT("sensors: coap put handler\r\n");
    // cbor to json
    size_t encoded_size = request->payload_length;
    cbor_to_json_string(request->payload, encoded_size, resource_json_str, RESOURCE_JSON_STR_SIZE, &encoded_size, 0);        
    return sensors_put_json_str(resource_json_str);
}

bool sensors_coap_handler(const coap_message_t *request, coap_message_t *response) {
    // SYS_CONSOLE_PRINT("sensors: coap handler\r\n");
    if(request->code == COAP_CODE_GET){
        return sensors_coap_get_handler(response);
    }
    else if(request->code == COAP_CODE_PUT){
        return sensors_coap_put_handler(request, response);
    }
    return false;
}

char * sensors_sensor_get_json_str(uint8_t channel) {
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor_get_json_str: channel out of range: %d\r\n", channel);
        return NULL;
    }
    cJSON * root = cJSON_CreateObject();
    uint8_t i = channel - 1;
    cJSON_AddNumberToObject(root,"channel",sensors[i].channel);
    cJSON_AddStringToObject(root,"cluster",sensors[i].cluster);
    cJSON_AddStringToObject(root,"prphtag",sensors[i].prphtag);
    cJSON_AddStringToObject(root,"type",sensors[i].type);
    cJSON_AddStringToObject(root,"eventlh",sensors[i].eventlh);
    cJSON_AddStringToObject(root,"eventhl",sensors[i].eventhl);
    cJSON_AddNumberToObject(root,"holdtime",sensors[i].holdtime);
    cJSON_AddNumberToObject(root,"occupiedtimeout",sensors[i].occupiedtimeout);
    cJSON_AddNumberToObject(root,"vaccanttimeout",sensors[i].vaccanttimeout);
    cJSON_AddNumberToObject(root,"high_threshold",sensors[i].high_threshold);
    cJSON_AddNumberToObject(root,"low_threshold",sensors[i].low_threshold);    
    cJSON_AddNumberToObject(root,"voltage",sense_get_sensor_voltage(channel));
    cJSON_AddStringToObject(root,"input_state",sensors[i].input_state ? "true" : "false");
    cJSON_AddStringToObject(root,"logical_state",sensors[i].logical_state ? "true" : "false");
    char * print_str = cJSON_PrintUnformatted(root);
    strncpy(resource_json_str,print_str,RESOURCE_JSON_STR_SIZE);
    cJSON_free(print_str);
    cJSON_Delete(root);
    return resource_json_str;
}

bool sensors_sensor_coap_get_handler(uint8_t channel, coap_message_t *response){
    SYS_CONSOLE_PRINT("sensors: sensor%u coap get handler\r\n", channel);
    snprintf(resource_e_json_str,RESOURCE_E_JSON_STR_SIZE,"{\"e\":%s}",sensors_sensor_get_json_str(channel));
    size_t encoded_size = 0;
    CborError error = json_to_cbor(resource_e_json_str, resource_cbor_buffer, RESOURCE_CBOR_BUFFER_SIZE, &encoded_size);
    if (error != CborNoError) {
        SYS_CONSOLE_PRINT("sensors: json_to_cbor error: %d\r\n", error);
        return false;      
    }
    
    response->code = COAP_CODE_CONTENT;
    response->content_format = COAP_CONTENT_FORMAT_APPLICATION_CBOR;
    response->payload_length = encoded_size;
    memcpy(response->payload, resource_cbor_buffer, response->payload_length);

    // SYS_CONSOLE_PRINT("sensors: response: %s\r\n", resource_e_json_str);
    // break long print into parts, split on characters
    SYS_CONSOLE_PRINT("sensors: response: ");
    int len = strlen(resource_e_json_str);
    for(int i = 0; i < len; i++){
        SYS_CONSOLE_PRINT("%c", resource_e_json_str[i]);
    }    
    SYS_CONSOLE_PRINT("\r\n");
    return true;
}

char * sensors_sensor_context_get_json_str(uint8_t channel) {
    cJSON * root = cJSON_CreateObject();
    cJSON * keyw_array = cJSON_CreateArray();    
    
    // add cluster to keyw_array
    cJSON_AddItemToArray(keyw_array, cJSON_CreateString(sensors[channel-1].cluster));
    
    cJSON_AddItemToObject(root, "keyw", keyw_array);
    char * print_str = cJSON_PrintUnformatted(root);
    strncpy(resource_json_str,print_str,RESOURCE_JSON_STR_SIZE);
    cJSON_free(print_str);
    cJSON_Delete(root);
    return resource_json_str;
}

bool sensors_sensor_context_coap_get_handler(uint8_t channel, coap_message_t *response){
    SYS_CONSOLE_PRINT("sensors: sensor%u context coap get handler\r\n", channel);
    snprintf(resource_e_json_str,RESOURCE_E_JSON_STR_SIZE,"{\"e\":%s}",sensors_sensor_context_get_json_str(channel));
    size_t encoded_size = 0;
    CborError error = json_to_cbor(resource_e_json_str, resource_cbor_buffer, RESOURCE_CBOR_BUFFER_SIZE, &encoded_size);
    if (error != CborNoError) {
        SYS_CONSOLE_PRINT("sensors: json_to_cbor error: %d\r\n", error);
        return false;      
    }
    
    response->code = COAP_CODE_CONTENT;
    response->content_format = COAP_CONTENT_FORMAT_APPLICATION_CBOR;
    response->payload_length = encoded_size;
    memcpy(response->payload, resource_cbor_buffer, response->payload_length);

    // SYS_CONSOLE_PRINT("sensors: response: %s\r\n", resource_e_json_str);
    // break long print into parts, split on characters
    SYS_CONSOLE_PRINT("sensors: response: ");
    int len = strlen(resource_e_json_str);
    for(int i = 0; i < len; i++){
        SYS_CONSOLE_PRINT("%c", resource_e_json_str[i]);
    }    
    SYS_CONSOLE_PRINT("\r\n");
    return true;
}

bool sensors_sensor_set_cluster(uint8_t channel, char *cluster){
    // TODO: validate cluster
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u set_cluster: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(sensors[i].cluster,cluster,16) != 0;
    strncpy(sensors[i].cluster,cluster,16);
    SYS_CONSOLE_PRINT("sensors: sensor%u cluster: %s\r\n", channel, sensors[i].cluster);
    if(changed){
        return storage_setStr(sensors[i].ns, "cluster", sensors[i].cluster);
    }
    return true;
}

bool sensors_sensor_set_prphtag(uint8_t channel, char *prphtag){
    // TODO: validate prphtag
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u set_prphtag: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(sensors[i].prphtag,prphtag,16) != 0;
    strncpy(sensors[i].prphtag,prphtag,16);
    SYS_CONSOLE_PRINT("sensors: sensor%u prphtag: %s\r\n", channel, sensors[i].prphtag);
    if(changed){
        return storage_setStr(sensors[i].ns, "prphtag", sensors[i].prphtag);
    }
    return true;
}

bool sensors_sensor_set_type(uint8_t channel, char *type){
    // TODO: validate type
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u set_type: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(sensors[i].type,type,16) != 0;
    strncpy(sensors[i].type,type,16);
    SYS_CONSOLE_PRINT("sensors: sensor%u type: %s\r\n", channel, sensors[i].type);
    if(changed){
        return storage_setStr(sensors[i].ns, "type", sensors[i].type);
    }
    return true;
}

bool sensors_sensor_set_eventlh(uint8_t channel, char *eventlh){
    // TODO: validate eventlh
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u set_eventlh: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(sensors[i].eventlh,eventlh,16) != 0;
    strncpy(sensors[i].eventlh,eventlh,16);
    SYS_CONSOLE_PRINT("sensors: sensor%u eventlh: %s\r\n", channel, sensors[i].eventlh);
    if(changed){
        return storage_setStr(sensors[i].ns, "eventlh", sensors[i].eventlh);
    }
    return true;
}

bool sensors_sensor_set_eventhl(uint8_t channel, char *eventhl){
    // TODO: validate eventhl
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u set_eventhl: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(sensors[i].eventhl,eventhl,16) != 0;
    strncpy(sensors[i].eventhl,eventhl,16);
    SYS_CONSOLE_PRINT("sensors: sensor%u eventhl: %s\r\n", channel, sensors[i].eventhl);
    if(changed){
        return storage_setStr(sensors[i].ns, "eventhl", sensors[i].eventhl);
    }
    return true;
}

bool sensors_sensor_set_holdtime(uint8_t channel, uint16_t holdtime){
    // TODO: validate holdtime
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u set_holdtime: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = sensors[i].holdtime != holdtime;
    sensors[i].holdtime = holdtime;
    SYS_CONSOLE_PRINT("sensors: sensor%u holdtime: %u\r\n", channel, sensors[i].holdtime);
    if(changed){
        return storage_setU16(sensors[i].ns, "holdtime", sensors[i].holdtime);
    }
    return true;
}

bool sensors_sensor_set_occupiedtimeout(uint8_t channel, uint16_t occupiedtimeout){
    // TODO: validate occupiedtimeout
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u set_occupiedtimeout: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = sensors[i].occupiedtimeout != occupiedtimeout;
    sensors[i].occupiedtimeout = occupiedtimeout;
    SYS_CONSOLE_PRINT("sensors: sensor%u occupiedtimeout: %u\r\n", channel, sensors[i].occupiedtimeout);
    if(changed){
        return storage_setU16(sensors[i].ns, "occupiedtimeout", sensors[i].occupiedtimeout);
    }
    return true;
}

bool sensors_sensor_set_vaccanttimeout(uint8_t channel, uint16_t vaccanttimeout){
    // TODO: validate vaccanttimeout
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u set_vaccanttimeout: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = sensors[i].vaccanttimeout != vaccanttimeout;
    sensors[i].vaccanttimeout = vaccanttimeout;
    SYS_CONSOLE_PRINT("sensors: sensor%u vaccanttimeout: %u\r\n", channel, sensors[i].vaccanttimeout);
    if(changed){
        return storage_setU16(sensors[i].ns, "vaccanttimeout", sensors[i].vaccanttimeout);
    }
    return true;
}

bool sensors_sensor_set_high_threshold(uint8_t channel, uint16_t high_threshold){
    // TODO: validate high_threshold
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u set_high_threshold: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = sensors[i].high_threshold != high_threshold;
    sensors[i].high_threshold = high_threshold;
    SYS_CONSOLE_PRINT("sensors: sensor%u high_threshold: %u\r\n", channel, sensors[i].high_threshold);
    if(changed){
        return storage_setU16(sensors[i].ns, "high_threshold", sensors[i].high_threshold);
    }
    return true;
}

bool sensors_sensor_set_low_threshold(uint8_t channel, uint16_t low_threshold){
    // TODO: validate low_threshold
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u set_low_threshold: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = sensors[i].low_threshold != low_threshold;
    sensors[i].low_threshold = low_threshold;
    SYS_CONSOLE_PRINT("sensors: sensor%u low_threshold: %u\r\n", channel, sensors[i].low_threshold);
    if(changed){
        return storage_setU16(sensors[i].ns, "low_threshold", sensors[i].low_threshold);
    }
    return true;
}

bool sensors_sensor_set_input_state(uint8_t channel, bool input_state){

    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u set_input_state: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = sensors[i].input_state != input_state;
    sensors[i].input_state = input_state;
    SYS_CONSOLE_PRINT("sensors: sensor%u input_state: %s\r\n", channel, sensors[i].input_state ? "true" : "false");
    // if(changed){
    //     return storage_setBool(sensors[i].ns, "input_state", sensors[i].input_state);
    // }
    return true;
}

bool sensors_sensor_set_logical_state(uint8_t channel, bool logical_state){
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u set_logical_state: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = sensors[i].logical_state != logical_state;
    sensors[i].logical_state = logical_state;
    SYS_CONSOLE_PRINT("sensors: sensor%u logical_state: %s\r\n", channel, sensors[i].logical_state ? "true" : "false");
    // if(changed){
    //     return storage_setBool(sensors[i].ns, "logical_state", sensors[i].logical_state);
    // }
    return true;
}

bool sensors_sensor_put_json_str(uint8_t channel, char * json_str){
    SYS_CONSOLE_PRINT("sensors: sensor%u put json str: %s\r\n", channel, json_str);
    // parse json
    bool ret = true;
    bool found = false;
    cJSON * root = cJSON_Parse(json_str);
    cJSON * map = root;
    cJSON * e = cJSON_GetObjectItem(root,"e");    
    if(e){
        map = e;
    }
    cJSON * cluster = cJSON_GetObjectItem(map,"cluster");    
    if(cluster){
        ret &= sensors_sensor_set_cluster(channel,cluster->valuestring);
        found = true;
    }
    cJSON * prphtag = cJSON_GetObjectItem(map,"prphtag");    
    if(prphtag){
        ret &= sensors_sensor_set_prphtag(channel,prphtag->valuestring);
        found = true;
    }
    cJSON * type = cJSON_GetObjectItem(map,"type");    
    if(type){
        ret &= sensors_sensor_set_type(channel,type->valuestring);
        found =true;
    }
    cJSON * eventlh = cJSON_GetObjectItem(map,"eventlh");    
    if(eventlh){
        ret &= sensors_sensor_set_eventlh(channel,eventlh->valuestring);
        found = true;
    }
    cJSON * eventhl = cJSON_GetObjectItem(map,"eventhl");    
    if(eventhl){
        ret &= sensors_sensor_set_eventhl(channel,eventhl->valuestring);
        found = true;
    }
    cJSON * holdtime = cJSON_GetObjectItem(map,"holdtime");    
    if(holdtime){
        ret &= sensors_sensor_set_holdtime(channel,holdtime->valueint);
        found = true;
    }
    cJSON * occupiedtimeout = cJSON_GetObjectItem(map,"occupiedtimeout");    
    if(occupiedtimeout){
        ret &= sensors_sensor_set_occupiedtimeout(channel,occupiedtimeout->valueint);
        found = true;
    }
    cJSON * vaccanttimeout = cJSON_GetObjectItem(map,"vaccanttimeout");    
    if(vaccanttimeout){
        ret &= sensors_sensor_set_vaccanttimeout(channel,vaccanttimeout->valueint);
        found = true;
    }
    cJSON * high_threshold = cJSON_GetObjectItem(map,"high_threshold");    
    if(high_threshold){
        ret &= sensors_sensor_set_high_threshold(channel,high_threshold->valueint);
        found = true;
    }
    cJSON * low_threshold = cJSON_GetObjectItem(map,"low_threshold");    
    if(low_threshold){
        ret &= sensors_sensor_set_low_threshold(channel,low_threshold->valueint);
        found = true;
    }   
    ret &= found;
    cJSON_Delete(root);
    return ret;
}

bool sensors_sensor_coap_put_handler(uint8_t channel, const coap_message_t *request, coap_message_t *response){
    SYS_CONSOLE_PRINT("sensors: sensor%u coap put handler\r\n", channel);
    // cbor to json
    size_t encoded_size = request->payload_length;
    cbor_to_json_string(request->payload, encoded_size, resource_json_str, RESOURCE_JSON_STR_SIZE, &encoded_size, 0);        
    return sensors_sensor_put_json_str(channel, resource_json_str);
}

bool sensors_sensor_coap_handler(const coap_message_t *request, coap_message_t *response) {
    // parse the uri
    char uri[64];
    uint8_t channel = 0;
    if(coap_parse_uri(request, uri, sizeof(uri))){
        SYS_CONSOLE_PRINT("sensors: sensor coap handler: %s\r\n", uri);
        sscanf(uri, "/inx/sensors/sensor%hhu", &channel);
        SYS_CONSOLE_PRINT("sensors: sensor%u coap handler\r\n", channel);
    }
    else{
        SYS_CONSOLE_PRINT("sensors: sensor coap handler: failed to parse uri\r\n");
        return false;
    }
    // SYS_CONSOLE_PRINT("actuators: actuator%u coap handler\r\n", channel);
    if(request->code == COAP_CODE_GET){
        return sensors_sensor_coap_get_handler(channel, response);
    }
    else if(request->code == COAP_CODE_PUT){
        return sensors_sensor_coap_put_handler(channel, request, response);
    }
    return false;
}

bool sensors_sensor_context_put_json_str(uint8_t channel, char * json_str){
    SYS_CONSOLE_PRINT("sensors: sensor%u context put json str: %s\r\n", channel, json_str);
    // parse json
    bool ret = true;
    bool found = false;
    cJSON * root = cJSON_Parse(json_str);
    cJSON * map = root;
    cJSON * e = cJSON_GetObjectItem(root,"e");    
    if(e){
        map = e;
    }
    cJSON * keyw = cJSON_GetObjectItem(map,"keyw");    
    if(keyw && cJSON_IsArray(keyw) && cJSON_GetArraySize(keyw) > 0){
        cJSON * first_element = cJSON_GetArrayItem(keyw, 0);
        if(first_element && cJSON_IsString(first_element)){
            ret &= sensors_sensor_set_cluster(channel, first_element->valuestring);
            found = true;
        }
    }
    ret &= found;
    cJSON_Delete(root);
    return ret;
}

bool sensors_sensor_context_coap_put_handler(uint8_t channel, const coap_message_t *request, coap_message_t *response){
    SYS_CONSOLE_PRINT("sensors: sensor%u context coap put handler\r\n", channel);
    // cbor to json
    size_t encoded_size = request->payload_length;
    cbor_to_json_string(request->payload, encoded_size, resource_json_str, RESOURCE_JSON_STR_SIZE, &encoded_size, 0);        
    return sensors_sensor_context_put_json_str(channel, resource_json_str);
}

bool sensors_sensor_context_coap_handler(const coap_message_t *request, coap_message_t *response) {
    // parse the uri
    char uri[64];
    uint8_t channel = 0;
    if(coap_parse_uri(request, uri, sizeof(uri))){
        SYS_CONSOLE_PRINT("sensors: sensor context coap handler: %s\r\n", uri);
        sscanf(uri, "/inx/sensors/sensor%hhu/context", &channel);
    }
    else{
        SYS_CONSOLE_PRINT("sensors: sensor context coap handler: failed to parse uri\r\n");
        return false;
    }
    SYS_CONSOLE_PRINT("sensors: sensor%u context coap handler\r\n", channel);
    if(request->code == COAP_CODE_GET){
        return sensors_sensor_context_coap_get_handler(channel, response);
    }
    else if(request->code == COAP_CODE_PUT){
        return sensors_sensor_context_coap_put_handler(channel, request, response);
    }
    return false;
}

char * sensors_sensor_get_cluster(uint8_t channel){
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u get_cluster: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return sensors[channel-1].cluster;
}

char * sensors_sensor_get_prphtag(uint8_t channel){
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u get_prphtag: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return sensors[channel-1].prphtag;
}

char * sensors_sensor_get_type(uint8_t channel){
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u get_type: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return sensors[channel-1].type;
}

char * sensors_sensor_get_eventlh(uint8_t channel){
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u get_eventlh: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return sensors[channel-1].eventlh;
}

char * sensors_sensor_get_eventhl(uint8_t channel){
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u get_eventhl: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return sensors[channel-1].eventhl;
}

uint16_t sensors_sensor_get_holdtime(uint8_t channel){
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u get_holdtime: channel out of range: %d\r\n", channel);
        return 0;
    }
    return sensors[channel-1].holdtime;
}

uint16_t sensors_sensor_get_occupiedtimeout(uint8_t channel){
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u get_occupiedtimeout: channel out of range: %d\r\n", channel);
        return 0;
    }
    return sensors[channel-1].occupiedtimeout;
}

uint16_t sensors_sensor_get_vaccanttimeout(uint8_t channel){
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u get_vaccanttimeout: channel out of range: %d\r\n", channel);
        return 0;
    }
    return sensors[channel-1].vaccanttimeout;
}

uint16_t sensors_sensor_get_high_threshold(uint8_t channel){
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u get_high_threshold: channel out of range: %d\r\n", channel);
        return 0;
    }
    return sensors[channel-1].high_threshold;
}

uint16_t sensors_sensor_get_low_threshold(uint8_t channel){ 
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u get_low_threshold: channel out of range: %d\r\n", channel);
        return 0;
    }
    return sensors[channel-1].low_threshold;
}

bool sensors_sensor_get_input_state(uint8_t channel){   
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u get_input_state: channel out of range: %d\r\n", channel);
        return false;
    }
    return sensors[channel-1].input_state;
}

bool sensors_sensor_get_logical_state(uint8_t channel){
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u get_logical_state: channel out of range: %d\r\n", channel);
        return false;
    }
    return sensors[channel-1].logical_state;
}

uint16_t sensors_sensor_get_voltage(uint8_t channel){
    if(channel<1 || channel>NUM_SENSORS){
        SYS_CONSOLE_PRINT("sensors: sensor%u get_voltage: channel out of range: %d\r\n", channel);
        return 0;
    }
    return sense_get_sensor_voltage(channel);
}

char * sensors_wallswitch_get_json_str(uint8_t channel) {
    if(channel<1 || channel>NUM_WALLSWITCHES){
        SYS_CONSOLE_PRINT("sensors: wallswitch_get_json_str: channel out of range: %d\r\n", channel);
        return NULL;
    }
    cJSON * root = cJSON_CreateObject();
    uint8_t i = channel - 1;
    cJSON_AddNumberToObject(root,"channel",wallswitches[i].channel);
    cJSON_AddStringToObject(root,"cluster",wallswitches[i].cluster);
    cJSON_AddStringToObject(root,"prphtag",wallswitches[i].prphtag);
    char * print_str = cJSON_PrintUnformatted(root);
    strncpy(resource_json_str,print_str,RESOURCE_JSON_STR_SIZE);
    cJSON_free(print_str);
    cJSON_Delete(root);
    return resource_json_str;
}

bool sensors_wallswitch_coap_get_handler(uint8_t channel, coap_message_t *response){
    SYS_CONSOLE_PRINT("sensors: wallswitch%u coap get handler\r\n", channel);
    snprintf(resource_e_json_str,RESOURCE_E_JSON_STR_SIZE,"{\"e\":%s}",sensors_wallswitch_get_json_str(channel));
    size_t encoded_size = 0;
    CborError error = json_to_cbor(resource_e_json_str, resource_cbor_buffer, RESOURCE_CBOR_BUFFER_SIZE, &encoded_size);
    if (error != CborNoError) {
        SYS_CONSOLE_PRINT("sensors: json_to_cbor error: %d\r\n", error);
        return false;      
    }
    
    response->code = COAP_CODE_CONTENT;
    response->content_format = COAP_CONTENT_FORMAT_APPLICATION_CBOR;
    response->payload_length = encoded_size;
    memcpy(response->payload, resource_cbor_buffer, response->payload_length);

    // SYS_CONSOLE_PRINT("sensors: response: %s\r\n", resource_e_json_str);
    // break long print into parts, split on characters
    SYS_CONSOLE_PRINT("sensors: response: ");
    int len = strlen(resource_e_json_str);
    for(int i = 0; i < len; i++){
        SYS_CONSOLE_PRINT("%c", resource_e_json_str[i]);
    }    
    SYS_CONSOLE_PRINT("\r\n");
    return true;
}

char * sensors_wallswitch_context_get_json_str(uint8_t channel) {
    cJSON * root = cJSON_CreateObject();
    cJSON * keyw_array = cJSON_CreateArray();    
    
    // add cluster to keyw_array
    cJSON_AddItemToArray(keyw_array, cJSON_CreateString(wallswitches[channel-1].cluster));
    
    cJSON_AddItemToObject(root, "keyw", keyw_array);
    char * print_str = cJSON_PrintUnformatted(root);
    strncpy(resource_json_str,print_str,RESOURCE_JSON_STR_SIZE);
    cJSON_free(print_str);
    cJSON_Delete(root);
    return resource_json_str;
}

bool sensors_wallswitch_context_coap_get_handler(uint8_t channel, coap_message_t *response){
    SYS_CONSOLE_PRINT("sensors: wallswitch%u context coap get handler\r\n", channel);
    snprintf(resource_e_json_str,RESOURCE_E_JSON_STR_SIZE,"{\"e\":%s}",sensors_wallswitch_context_get_json_str(channel));
    size_t encoded_size = 0;
    CborError error = json_to_cbor(resource_e_json_str, resource_cbor_buffer, RESOURCE_CBOR_BUFFER_SIZE, &encoded_size);
    if (error != CborNoError) {
        SYS_CONSOLE_PRINT("sensors: json_to_cbor error: %d\r\n", error);
        return false;      
    }
    
    response->code = COAP_CODE_CONTENT;
    response->content_format = COAP_CONTENT_FORMAT_APPLICATION_CBOR;
    response->payload_length = encoded_size;
    memcpy(response->payload, resource_cbor_buffer, response->payload_length);

    // SYS_CONSOLE_PRINT("sensors: response: %s\r\n", resource_e_json_str);
    // break long print into parts, split on characters
    SYS_CONSOLE_PRINT("sensors: response: ");
    int len = strlen(resource_e_json_str);
    for(int i = 0; i < len; i++){
        SYS_CONSOLE_PRINT("%c", resource_e_json_str[i]);
    }    
    SYS_CONSOLE_PRINT("\r\n");
    return true;
}

bool sensors_wallswitch_set_cluster(uint8_t channel, char *cluster){
    // TODO: validate cluster
    if(channel<1 || channel>NUM_WALLSWITCHES){
        SYS_CONSOLE_PRINT("sensors: wallswitch%u set_cluster: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(wallswitches[i].cluster,cluster,16) != 0;
    strncpy(wallswitches[i].cluster,cluster,16);
    SYS_CONSOLE_PRINT("sensors: wallswitch%u cluster: %s\r\n", channel, wallswitches[i].cluster);
    if(changed){
        return storage_setStr(wallswitches[i].ns, "cluster", wallswitches[i].cluster);
    }
    return true;
}

bool sensors_wallswitch_set_prphtag(uint8_t channel, char *prphtag){
    // TODO: validate prphtag
    if(channel<1 || channel>NUM_WALLSWITCHES){
        SYS_CONSOLE_PRINT("sensors: wallswitch%u set_prphtag: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(wallswitches[i].prphtag,prphtag,16) != 0;
    strncpy(wallswitches[i].prphtag,prphtag,16);
    SYS_CONSOLE_PRINT("sensors: wallswitch%u prphtag: %s\r\n", channel, wallswitches[i].prphtag);
    if(changed){
        return storage_setStr(wallswitches[i].ns, "prphtag", wallswitches[i].prphtag);
    }
    return true;
}

bool sensors_wallswitch_put_json_str(uint8_t channel, char * json_str){
    SYS_CONSOLE_PRINT("sensors: wallswitch%u put json str: %s\r\n", channel, json_str);
    // parse json
    bool ret = true;
    bool found = false;
    cJSON * root = cJSON_Parse(json_str);
    cJSON * map = root;
    cJSON * e = cJSON_GetObjectItem(root,"e");    
    if(e){
        map = e;
    }
    cJSON * cluster = cJSON_GetObjectItem(map,"cluster");    
    if(cluster){
        ret &= sensors_wallswitch_set_cluster(channel,cluster->valuestring);
        found = true;
    }
    cJSON * prphtag = cJSON_GetObjectItem(map,"prphtag");    
    if(prphtag){
        ret &= sensors_wallswitch_set_prphtag(channel,prphtag->valuestring);
        found = true;
    } 
    ret &= found;
    cJSON_Delete(root);
    return ret;
}

bool sensors_wallswitch_coap_put_handler(uint8_t channel, const coap_message_t *request, coap_message_t *response){
    SYS_CONSOLE_PRINT("sensors: wallswitch%u coap put handler\r\n", channel);
    // cbor to json
    size_t encoded_size = request->payload_length;
    cbor_to_json_string(request->payload, encoded_size, resource_json_str, RESOURCE_JSON_STR_SIZE, &encoded_size, 0);        
    return sensors_wallswitch_put_json_str(channel, resource_json_str);
}

bool sensors_wallswitch_coap_handler(const coap_message_t *request, coap_message_t *response) {
    // parse the uri
    char uri[64];
    uint8_t channel = 0;
    if(coap_parse_uri(request, uri, sizeof(uri))){
        SYS_CONSOLE_PRINT("sensors: wallswitch coap handler: %s\r\n", uri);
        // sscanf(uri, "/inx/sensors/wallswitch%hhu", &channel);
        channel = 1;
        SYS_CONSOLE_PRINT("sensors: wallswitch%u coap handler\r\n", channel);
    }
    else{
        SYS_CONSOLE_PRINT("sensors: wallswitch coap handler: failed to parse uri\r\n");
        return false;
    }
    // SYS_CONSOLE_PRINT("sensors: wallswitch%u coap handler\r\n", channel);
    if(request->code == COAP_CODE_GET){
        return sensors_wallswitch_coap_get_handler(channel, response);
    }
    else if(request->code == COAP_CODE_PUT){
        return sensors_wallswitch_coap_put_handler(channel, request, response);
    }
    return false;
}

bool sensors_wallswitch_context_put_json_str(uint8_t channel, char * json_str){
    SYS_CONSOLE_PRINT("sensors: wallswitch%u context put json str: %s\r\n", channel, json_str);
    // parse json
    bool ret = true;
    bool found = false;
    cJSON * root = cJSON_Parse(json_str);
    cJSON * map = root;
    cJSON * e = cJSON_GetObjectItem(root,"e");    
    if(e){
        map = e;
    }
    cJSON * keyw = cJSON_GetObjectItem(map,"keyw");    
    if(keyw && cJSON_IsArray(keyw) && cJSON_GetArraySize(keyw) > 0){
        cJSON * first_element = cJSON_GetArrayItem(keyw, 0);
        if(first_element && cJSON_IsString(first_element)){
            ret &= sensors_wallswitch_set_cluster(channel, first_element->valuestring);
            found = true;
        }
    }
    ret &= found;
    cJSON_Delete(root);
    return ret;
}

bool sensors_wallswitch_context_coap_put_handler(uint8_t channel, const coap_message_t *request, coap_message_t *response){
    SYS_CONSOLE_PRINT("sensors: wallswitch%u context coap put handler\r\n", channel);
    // cbor to json
    size_t encoded_size = request->payload_length;
    cbor_to_json_string(request->payload, encoded_size, resource_json_str, RESOURCE_JSON_STR_SIZE, &encoded_size, 0);        
    return sensors_wallswitch_context_put_json_str(channel, resource_json_str);
}

bool sensors_wallswitch_context_coap_handler(const coap_message_t *request, coap_message_t *response) {
    // parse the uri
    char uri[64];
    uint8_t channel = 0;
    if(coap_parse_uri(request, uri, sizeof(uri))){
        SYS_CONSOLE_PRINT("sensors: wallswitch context coap handler: %s\r\n", uri);
        // sscanf(uri, "/inx/sensors/wallswitch%hhu/context", &channel);
        channel = 1;
    }
    else{
        SYS_CONSOLE_PRINT("sensors: wallswitch context coap handler: failed to parse uri\r\n");
        return false;
    }
    SYS_CONSOLE_PRINT("sensors: wallswitch%u context coap handler\r\n", channel);
    if(request->code == COAP_CODE_GET){
        return sensors_wallswitch_context_coap_get_handler(channel, response);
    }
    else if(request->code == COAP_CODE_PUT){
        return sensors_wallswitch_context_coap_put_handler(channel, request, response);
    }
    return false;
}

char * sensors_wallswitch_get_cluster(uint8_t channel){
    if(channel<1 || channel>NUM_WALLSWITCHES){
        SYS_CONSOLE_PRINT("sensors: wallswitch%u get_cluster: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return wallswitches[channel-1].cluster;
}

char * sensors_wallswitch_get_prphtag(uint8_t channel){
    if(channel<1 || channel>NUM_WALLSWITCHES){
        SYS_CONSOLE_PRINT("sensors: wallswitch%u get_prphtag: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return wallswitches[channel-1].prphtag;
}