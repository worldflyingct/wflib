#include <stdio.h>
#include "wfasyncio.h"
#include "wfcoroutine.h"

#define STACKSIZE   2048

static WF_CTX {
    ucontext_t ctx;
    void *ptr;
    static WF_CTX *tail;
};
static WF_CTX *remainhead = NULL;
ucontext_t main_ctx;

int Wf_Create_Coroutine (Wf_Run_Coroutine runcorutine, void *ptr) {
    static WF_CTX* wf_ctx;
    if (remainhead != NULL) {
        wf_ctx = remainhead;
        remainhead = remainhead->tail;
    } else {
        wf_ctx = (static WF_CTX*)malloc(sizeof(static WF_CTX));
        if (coroutine == NULL) {
            perror("malloc coroutine fail");
            printf("in %s, at %d\n", __FILE__, __LINE__);
            return -1;
        }
        getcontext(&wf_ctx->ctx);
        wf_ctx->ctx.uc_stack.ss_sp = (char*)malloc(STACKSIZE * sizeof(char));
        if (wf_ctx->ctx.uc_stack.ss_sp == NULL) {
            perror("malloc coroutine stack fail");
            printf("in %s, at %d\n", __FILE__, __LINE__);
            return -2;
        }
        wf_ctx->ctx.uc_stack.ss_size = STACKSIZE;
        wf_ctx->ctx.uc_link = &main_ctx;
    }
    if (makecontext(&wf_ctx->ctx, runcorutine, 2, &wf_ctx->ctx, ptr)) {
        perror("makecontext fail");
        printf("in %s, at %d\n", __FILE__, __LINE__);
        return -3;
    }
    return 0;
}

int Delete_Coroutine (static WF_CTX* wf_ctx) {
    wf_ctx->tail = remainhead;
    remainhead = wf_ctx;
    if (setcontext(&main_ctx)) {
        perror("delete coroutine fail");
        printf("in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

int Suspend_Coroutine (static WF_CTX* wf_ctx) {
    if (swapcontext(&main_ctx, &wf_ctx->ctx)) {
        perror("suspend coroutine fail");
        printf("in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

int Resume_Coroutine (static WF_CTX* wf_ctx) {
    if (swapcontext(&wf_ctx->ctx, &main_ctx)) {
        perror("resume coroutine fail");
        printf("in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
    return 0;
}
