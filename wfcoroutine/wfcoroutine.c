#include <stdio.h>
#include <string.h>
#include "../wfasyncio/wfasyncio.h"
#include "wfcoroutine.h"

#define STACKSIZE   2048

struct WF_CTX {
    ucontext_t ctx;
    CORO_STATUS status;
    WF_NIO *asyncio;
    struct WF_CTX* tail;
};
WF_CTX *remainhead = NULL;
ucontext_t main_ctx;

int Coroutine_ReadFunc (WF_NIO *asyncio, int fd, void *ptr) {
    WF_CTX* wf_ctx = ptr;
    wf_ctx->status = READ;
    wf_ctx->asyncio = asyncio;
    if (swapcontext(&main_ctx, &wf_ctx->ctx)) {
        perror("suspend coroutine fail");
        printf("in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
}

int Coroutine_WriteFunc (WF_NIO *asyncio, int fd, void *ptr) {
    WF_CTX* wf_ctx = ptr;
    wf_ctx->status = WRITE;
    wf_ctx->asyncio = asyncio;
    if (swapcontext(&main_ctx, &wf_ctx->ctx)) {
        perror("suspend coroutine fail");
        printf("in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
}

int Coroutine_ErrorFunc (WF_NIO *asyncio, int fd, void *ptr, uint32_t events) {
    WF_CTX* wf_ctx = ptr;
    wf_ctx->status = ERROR;
    wf_ctx->asyncio = asyncio;
    if (swapcontext(&main_ctx, &wf_ctx->ctx)) {
        perror("suspend coroutine fail");
        printf("in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

int Wf_Create_Coroutine (Wf_Run_Coroutine runcorutine, int fd, int write_trigger, void* ptr) {
    WF_CTX* wf_ctx;
    if (remainhead != NULL) {
        wf_ctx = remainhead;
        remainhead = remainhead->tail;
    } else {
        wf_ctx = (WF_CTX*)malloc(sizeof(WF_CTX));
        if (wf_ctx == NULL) {
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
    makecontext(&wf_ctx->ctx, (void*)runcorutine, 4, wf_ctx, wf_ctx->asyncio, wf_ctx->status, ptr);
    Wf_Nio_WriteFunc wfn = write_trigger ? Coroutine_WriteFunc : NULL;
    if (Wf_Add_Epoll_Fd(fd, Coroutine_ReadFunc, wfn, Coroutine_ErrorFunc, wf_ctx)) {
        wf_ctx->tail = remainhead;
        remainhead = remainhead->tail;
    }
    return 0;
}

int Delete_Coroutine (WF_CTX* wf_ctx) {
    wf_ctx->tail = remainhead;
    remainhead = wf_ctx;
    if (Wf_Del_Epoll_Fd(wf_ctx->asyncio)) {
        printf("Wf_Del_Epoll_Fd fail, in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
    if (setcontext(&main_ctx)) {
        perror("delete coroutine fail");
        printf("in %s, at %d\n", __FILE__, __LINE__);
        return -2;
    }
    return 0;
}

int Suspend_Coroutine (WF_CTX* wf_ctx) {
    if (swapcontext(&main_ctx, &wf_ctx->ctx)) {
        perror("suspend coroutine fail");
        printf("in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

int Coroutine_ReadData (WF_CTX* wf_ctx, unsigned char *data, unsigned int size) {
    return Wf_Nio_Read_fd (wf_ctx->asyncio, data, size);
}

int Coroutine_WriteData (WF_CTX* wf_ctx, unsigned char *data, unsigned int size) {
    return Wf_Nio_Write_fd (wf_ctx->asyncio, data, size);
}
