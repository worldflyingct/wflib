#include <stdio.h>
#include "wfasyncio/wfasyncio.h"
#include "wfproxy/wfproxy.h"

/*
int ws_new_client (WFWS *wfws, void *ptr) {
    printf("have a new client, in %s, at %d\n", __FILE__, __LINE__);
}

int ws_receive_client (WFWS *wfws, unsigned char *data, unsigned long size, WS_DATA_TYPE ws_data_type, void *ptr) {
    if (ws_data_type == TEXT) {
        printf("text, size:%d, content:%s, in %s, at %d\n", size, data, __FILE__, __LINE__);
    } else {
        printf("blob, in %s, at %d\n", __FILE__, __LINE__);
    }
    Send_Ws_Data(wfws, "I am ws server!!!", sizeof("I am ws server!!!")-1, TEXT);
}

int ws_lose_client (WFWS *wfws, void *ptr) {
}
*/

int main () {
    Init_Wf_Nio_Io();
/*
    if (Wf_Nio_Create_Ws_Server(3888, 51200, ws_new_client, ws_receive_client, ws_lose_client, NULL)) {
        return -1;
    }
*/
    Init_Proxy (8889, 0, 51200);
    Wf_Run_Event(8192);
}
