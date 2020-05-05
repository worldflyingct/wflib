#ifndef __COROUTINE_H__
#define __COROUTINE_H__

#include <ucontext.h>
#include "../wfasyncio/wfasyncio.h"

typedef enum {
    READ,
    WRITE,
    ERROR,
    NONE
} CORO_STATUS;

typedef struct WF_CTX {
    ucontext_t* ctx;
    CORO_STATUS status;
    WF_NIO *asyncio;
    char* stack;
    unsigned int stacksize;
    ucontext_t* link;
    struct WF_CTX* tail;
} WF_CTX;
typedef void (*Wf_Run_Coroutine) (WF_CTX *wf_ctx, void* ptr);

int Wf_Create_Coroutine (Wf_Run_Coroutine runcorutine, int fd, int write_trigger, void *ptr);
int Wf_Mod_Coroutine (WF_CTX* wf_ctx, Wf_Run_Coroutine runcorutine, int write_trigger, void* ptr);
WF_CTX* Coroutine_Wait_Event (WF_CTX* wf_ctx, int fd, int read_trigger, int write_trigger);
int Delete_Coroutine (WF_CTX* wf_ctx);
int Suspend_Coroutine (WF_CTX* wf_ctx);
int Coroutine_ReadData (WF_CTX* wf_ctx, unsigned char *data, unsigned int size);
int Coroutine_WriteData (WF_CTX* wf_ctx, unsigned char *data, unsigned int size);

#endif
