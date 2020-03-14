#ifndef __WF_WS_H__
#define __WF_WS_H__

#include "wfasyncio.h"

typedef struct WFWS WFWS;
typedef int (*Wf_Nio_Ws_New_Socket) (WF_NIO *asyncio, int fd, void *ptr);

int Wf_Nio_Create_Ws_Server (unsigned short port, int max_connect);

#endif
