#include <stdio.h>
#include "wfasyncio.h"
#include "wfws.h"

int Ws_New_Socket (WF_NIO *asyncio, int fd, void *ptr) {
}

int main () {
    Init_Wf_Nio_Io();
    if (Wf_Nio_Create_Ws_Server(3888, 51200)) {
        return -1;
    }
    Wf_Run_Event(8192);
}
