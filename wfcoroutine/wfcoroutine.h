#ifndef __COROUTINE_H__
#define __COROUTINE_H__

#include <ucontext.h>

typedef void (*Wf_Run_Coroutine) (ucontext_t *ctx, void *ptr);

int Wf_Create_Coroutine (Wf_Run_Coroutine runcorutine, void* ptr);
int Delete_Coroutine (static WF_CTX* wf_ctx);
int Suspend_Coroutine (static WF_CTX* wf_ctx);
int Resume_Coroutine (static WF_CTX* wf_ctx);

#endif
