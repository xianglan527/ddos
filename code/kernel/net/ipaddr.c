#include "ipaddr.h"

void ipaddr_set_any(Ipaddr * ip) {
    ip->q_addr = 0;
}

int ipaddr_from_str(Ipaddr* dest, char* str){
    if(!dest || !str){
        return -E_NET_PARAM;
    }
    dest->q_addr = 0;
    char c;
    uint8_t *p = dest->a_addr;
    uint8_t sub_addr = 0;
    while((c = *str++) != '\0'){
        if((c >= '0') && (c <= '9')){
            sub_addr = sub_addr * 10 + c - '0';
        }else if(c == '.'){
            *p++ = sub_addr;
            sub_addr = 0;
        }else{
            return -E_NET_PARAM;
        }
    }
    *p++ = sub_addr;
    return NET_OK;
}

Ipaddr* ipaddr_get_any(void){
    static Ipaddr ipaddr_any = {.type = IPADDR_V4, .q_addr = 0};
    return &ipaddr_any;
}

void ipaddr_copy(Ipaddr* dest, Ipaddr* src){
    if (!dest || !src) { return; }
    dest->q_addr = src->q_addr;
    dest->type = src->type;
}