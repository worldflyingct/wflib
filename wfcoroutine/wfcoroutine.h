#ifndef __COROUTINE_H__
#define __COROUTINE_H__

#include <ucontext.h>

typedef enum {
    READ,
    WRITE,
    ERROR
} CORO_STATUS;

typedef struct WF_CTX WF_CTX;
typedef void (*Wf_Run_Coroutine) (WF_CTX *wf_ctx, CORO_STATUS status, void* ptr);

int Wf_Create_Coroutine (Wf_Run_Coroutine runcorutine, int fd, int write_trigger, void *ptr);
int Delete_Coroutine (WF_CTX* wf_ctx);
int Suspend_Coroutine (WF_CTX* wf_ctx);
int Coroutine_ReadData (WF_CTX* wf_ctx, unsigned char *data, unsigned int size);
int Coroutine_WriteData (WF_CTX* wf_ctx, unsigned char *data, unsigned int size);

#endif
