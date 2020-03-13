#include <stdio.h>
#include "wfasyncio.h"
#include "wfws.h"

int main () {
    Init_Wf_Async_Io();
    int fd = Create_Ws_Server(80, 51200);
    Wf_Add_Epoll_Fd(fd, Read_Ws_Socket, Write_Ws_Socket, Error_Ws_Socket, NULL);
    Wf_Run_Event(8192);
}
