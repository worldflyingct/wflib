#ifndef __WF_WS_H__
#define __WF_WS_H__

#include "wfasyncio.h"

typedef struct WFWS WFWS;

int Create_Ws_Server (unsigned short port, int max_connect);
int Read_Ws_Socket (WFASYNCIO *asyncio, int fd, void *ws);
int Write_Ws_Socket (WFASYNCIO *asyncio, int fd, void *ws);
int Error_Ws_Socket (WFASYNCIO *asyncio, int fd, void *ws, uint32_t events);

#endif
