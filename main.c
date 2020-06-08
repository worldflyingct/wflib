#include <stdio.h>
#include "wfasyncio/wfasyncio.h"
#include "wfhttp/wfhttp.h"

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

int http_required_handle (WFHTTP *wfhttp, int fd, void* ptr, unsigned char *body, unsigned int body_size, enum HTTPMETHOD httpmethod, unsigned char *path, unsigned int path_len, enum HTTPVERSION version, HTTPPARAM *httpparam, unsigned int httpparam_size) {
    printf("path:%s, in %s, at %d\n", path, __FILE__, __LINE__);
    for (unsigned int i = 0 ; i < httpparam_size ; i++) {
        printf("%s: %s\n", httpparam[i].key, httpparam[i].value);
    }
    Http_End(wfhttp, "hello wfhttpd", sizeof("hello wfhttpd") - 1);
}

int main () {
    Init_Wf_Nio_Io();
/*
    if (Wf_Nio_Create_Ws_Server(3888, 51200, ws_new_client, ws_receive_client, ws_lose_client, NULL)) {
        return -1;
    }
*/
    if (Wf_Nio_Create_Http_Server(8086, 51200, http_required_handle, NULL)) {
        return -1;
    }
    Wf_Run_Event(8192);
    return 0;
}
