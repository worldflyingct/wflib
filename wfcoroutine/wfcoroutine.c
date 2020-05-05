#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../wfasyncio/wfasyncio.h"
#include "wfcoroutine.h"

#define STACKSIZE   8*1024*1024

WF_CTX *remainhead = NULL;
ucontext_t main_ctx;

int Coroutine_ReadFunc (WF_NIO *asyncio, int fd, void *ptr) {
    WF_CTX* wf_ctx = ptr;
    wf_ctx->status = READ;
    wf_ctx->asyncio = asyncio;
    if (swapcontext(&main_ctx, wf_ctx->ctx)) {
        perror("suspend coroutine fail");
        printf("in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
}

int Coroutine_WriteFunc (WF_NIO *asyncio, int fd, void *ptr) {
    WF_CTX* wf_ctx = ptr;
    wf_ctx->status = WRITE;
    wf_ctx->asyncio = asyncio;
    if (swapcontext(&main_ctx, wf_ctx->ctx)) {
        perror("suspend coroutine fail");
        printf("in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
}

int Coroutine_ErrorFunc (WF_NIO *asyncio, int fd, void *ptr, uint32_t events) {
    WF_CTX* wf_ctx = ptr;
    wf_ctx->status = ERROR;
    wf_ctx->asyncio = asyncio;
    if (swapcontext(&main_ctx, wf_ctx->ctx)) {
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
        wf_ctx->asyncio = NULL;
        wf_ctx->ctx = NULL;
    }
    if (!wf_ctx->ctx) {
        ucontext_t* ctx = (ucontext_t*)malloc(sizeof(ucontext_t));
        if (ctx == NULL) {
            perror("malloc context fail");
            printf("in %s, at %d\n", __FILE__, __LINE__);
            return -2;
        }
        wf_ctx->ctx = ctx;
        char* sp = (char*)malloc(STACKSIZE * sizeof(char));
        if (sp == NULL) {
            perror("malloc coroutine stack fail");
            printf("in %s, at %d\n", __FILE__, __LINE__);
            return -3;
        }
        wf_ctx->stack = sp;
        wf_ctx->stacksize = STACKSIZE;
        wf_ctx->link = &main_ctx;
    }
    getcontext(wf_ctx->ctx);
    wf_ctx->ctx->uc_stack.ss_sp = wf_ctx->stack;
    wf_ctx->ctx->uc_stack.ss_size = wf_ctx->stacksize;
    wf_ctx->ctx->uc_link = &main_ctx;
    makecontext(wf_ctx->ctx, (void*)runcorutine, 2, wf_ctx, ptr);
    Wf_Nio_WriteFunc wfn = write_trigger ? Coroutine_WriteFunc : NULL;
    if (Wf_Add_Epoll_Fd(fd, Coroutine_ReadFunc, wfn, Coroutine_ErrorFunc, wf_ctx)) {
        wf_ctx->tail = remainhead;
        remainhead = remainhead->tail;
        return -4;
    }
    return 0;
}

WF_CTX* Coroutine_Wait_Event (WF_CTX* wf_oldctx, int fd, int read_trigger, int write_trigger) {
    WF_CTX* wf_ctx;
    if (remainhead != NULL) {
        wf_ctx = remainhead;
        remainhead = remainhead->tail;
    } else {
        wf_ctx = (WF_CTX*)malloc(sizeof(WF_CTX));
        if (wf_ctx == NULL) {
            perror("malloc coroutine fail");
            printf("in %s, at %d\n", __FILE__, __LINE__);
            return NULL;
        }
        wf_ctx->asyncio = NULL;
    }
    wf_ctx->ctx = wf_oldctx->ctx;
    Wf_Nio_WriteFunc rfn = read_trigger ? Coroutine_ReadFunc : NULL;
    Wf_Nio_WriteFunc wfn = write_trigger ? Coroutine_WriteFunc : NULL;
    if (Wf_Add_Epoll_Fd(fd, rfn, wfn, Coroutine_ErrorFunc, wf_ctx)) {
        wf_ctx->tail = remainhead;
        remainhead = remainhead->tail;
        return NULL;
    }
    wf_ctx->status = NONE;
    while (wf_ctx->status == NONE) {
        printf("in %s, at %d\n", __FILE__, __LINE__);
        if (wf_ctx->ctx == NULL || swapcontext(wf_ctx->ctx, &main_ctx)) {
            perror("suspend coroutine fail");
            printf("in %s, at %d\n", __FILE__, __LINE__);
            wf_ctx->ctx = NULL;
            Delete_Coroutine(wf_ctx);
            return NULL;
        }
        if (wf_oldctx->status == ERROR || wf_ctx->status == ERROR) {
            // printf("connect lose, in %s, at %d\n", __FILE__, __LINE__);
            Delete_Coroutine(wf_ctx);
            return NULL;
        }
    }
    wf_ctx->ctx = NULL;
    return wf_ctx;
}

int Wf_Mod_Coroutine (WF_CTX* wf_ctx, Wf_Run_Coroutine runcorutine, int write_trigger, void* ptr) {
    if (!wf_ctx->ctx) {
        ucontext_t* ctx = (ucontext_t*)malloc(sizeof(ucontext_t));
        if (ctx == NULL) {
            perror("malloc context fail");
            printf("in %s, at %d\n", __FILE__, __LINE__);
            return -1;
        }
        wf_ctx->ctx = ctx;
        char* sp = (char*)malloc(STACKSIZE * sizeof(char));
        if (sp == NULL) {
            perror("malloc coroutine stack fail");
            printf("in %s, at %d\n", __FILE__, __LINE__);
            return -2;
        }
        wf_ctx->stack = sp;
        wf_ctx->stacksize = STACKSIZE;
        wf_ctx->link = &main_ctx;
    }
    getcontext(wf_ctx->ctx);
    wf_ctx->ctx->uc_stack.ss_sp = wf_ctx->stack;
    wf_ctx->ctx->uc_stack.ss_size = wf_ctx->stacksize;
    wf_ctx->ctx->uc_link = &main_ctx;
    makecontext(wf_ctx->ctx, (void*)runcorutine, 2, wf_ctx, ptr);
    Wf_Nio_WriteFunc wfn = write_trigger ? Coroutine_WriteFunc : NULL;
    if (Wf_Mod_Epoll_Fd(wf_ctx->asyncio, Coroutine_ReadFunc, wfn, Coroutine_ErrorFunc, wf_ctx)) {
        return -3;
    }
    printf("in %s, at %d\n", __FILE__, __LINE__);
    return 0;
}

int Delete_Coroutine (WF_CTX* wf_ctx) {
    wf_ctx->tail = remainhead;
    remainhead = wf_ctx;
    if (wf_ctx->asyncio == NULL) {
        // printf("in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
    if (Wf_Del_Epoll_Fd(wf_ctx->asyncio)) {
        printf("Wf_Del_Epoll_Fd fail, in %s, at %d\n", __FILE__, __LINE__);
        return -2;
    }
    wf_ctx->asyncio = NULL;
    return 0;
}

int Suspend_Coroutine (WF_CTX* wf_ctx) {
    if (swapcontext(wf_ctx->ctx, &main_ctx)) {
        perror("suspend coroutine fail");
        printf("in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

int Coroutine_ReadData (WF_CTX* wf_ctx, unsigned char *data, unsigned int size) {
    if (wf_ctx->asyncio == NULL) {
        printf("in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
    return Wf_Nio_Read_fd (wf_ctx->asyncio, data, size);
}

int Coroutine_WriteData (WF_CTX* wf_ctx, unsigned char *data, unsigned int size) {
    if (wf_ctx->asyncio == NULL) {
        printf("in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
    return Wf_Nio_Write_fd (wf_ctx->asyncio, data, size);
}
