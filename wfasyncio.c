#include <stdio.h>
#include <stdlib.h>
#include "wfasyncio.h"

struct WFASYNCIO {
    int fd;
    ReadFunc readfn; // 遇到可写事件时触发的回调函数
    WriteFunc writefn; // 遇到可写事件时触发的回调函数
    ErrorFunc errorfn; // 遇到可写事件时触发的回调函数
    void* ptr;
};

static WFASYNCIO *remainhead = NULL;
static int epollfd = 0;
static int wait_count;

int Init_Wf_Async_Io () {
    if (epollfd > 0) {
        return 0;
    }
    if ((epollfd = epoll_create(1024)) < 0) { // epoll_create的参数在高版本中已经废弃了，填入一个大于0的数字都一样。
        perror("epoll_create fail");
        return -1;
    }
    return 0;
}

int Wf_Add_Epoll_Fd (int fd, ReadFunc readfn, WriteFunc writefn, ErrorFunc errorfn, void *ptr) {
    struct epoll_event ev;
    uint32_t flags = 0;
    if (readfn) {
        flags |= EPOLLIN;
    }
    if (writefn) {
        flags |= EPOLLOUT;
    }
    if (errorfn) {
        flags |= EPOLLERR | EPOLLHUP | EPOLLRDHUP;
    }
    ev.events = flags;
    WFASYNCIO *asyncio;
    if (remainhead == NULL) {
        if ((asyncio = (WFASYNCIO*)malloc(sizeof(WFASYNCIO))) == NULL) {
            perror("malloc new WFASYNCIO obj fail");
            return -1;
        }
    } else {
        asyncio = remainhead;
        remainhead = remainhead->ptr;
    }
    asyncio->fd = fd;
    asyncio->readfn = readfn;
    asyncio->writefn = writefn;
    asyncio->errorfn = errorfn;
    asyncio->ptr = ptr;
    ev.data.ptr = asyncio;
    return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

int Wf_Mod_Epoll_Fd (WFASYNCIO *asyncio, ReadFunc readfn, WriteFunc writefn, ErrorFunc errorfn, void *ptr) {
    struct epoll_event ev;
    uint32_t flags = 0;
    if (readfn) {
        flags |= EPOLLIN;
    }
    if (writefn) {
        flags |= EPOLLOUT;
    }
    if (errorfn) {
        flags |= EPOLLERR | EPOLLHUP | EPOLLRDHUP;
    }
    ev.events = flags;
    asyncio->readfn = readfn;
    asyncio->writefn = writefn;
    asyncio->errorfn = errorfn;
    asyncio->ptr = ptr;
    ev.data.ptr = asyncio;
    return epoll_ctl(epollfd, EPOLL_CTL_MOD, asyncio->fd, &ev);
}

int Wf_Del_Epoll_Fd (WFASYNCIO *asyncio) {
    asyncio->ptr = remainhead;
    remainhead = asyncio;
    return epoll_ctl(epollfd, EPOLL_CTL_DEL, asyncio->fd, NULL);
}

int Wf_Run_Event (int maxevents) {
    struct epoll_event *evs = (struct epoll_event*)malloc(maxevents * sizeof(struct epoll_event));
    if (evs == NULL) {
        perror("malloc epoll events fail");
        return -1;
    }
LOOP:
    wait_count = epoll_wait(epollfd, evs, maxevents, -1);
    for (int i = 0 ; i < wait_count ; i++) {
        WFASYNCIO *asyncio = evs[i].data.ptr;
        uint32_t events = evs[i].events;
        if (events & ~(EPOLLIN | EPOLLOUT)) { // 除了独写外，还有别的事件
            asyncio->errorfn(asyncio, asyncio->fd, asyncio->ptr, events);
        } else if (events & EPOLLIN) {
            asyncio->readfn(asyncio, asyncio->fd, asyncio->ptr);
        } else if (events & EPOLLOUT) {
            asyncio->writefn(asyncio, asyncio->fd, asyncio->ptr);
        }
    }
    goto LOOP;
}
