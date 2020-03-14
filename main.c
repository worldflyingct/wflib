#include <stdio.h>
#include "wfasyncio.h"
#include "wfws.h"

int main () {
    Init_Wf_Nio_Io();
    if (Wf_Nio_Create_Ws_Server(3888, 51200)) {
        return -1;
    }
    Wf_Run_Event(8192);
}
