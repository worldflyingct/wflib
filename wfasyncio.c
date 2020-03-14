#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include "wfasyncio.h"

struct WF_NIO {
    int fd;
    Wf_Nio_ReadFunc readfn; // 遇到可写事件时触发的回调函数
    Wf_Nio_WriteFunc writefn; // 遇到可写事件时触发的回调函数
    Wf_Nio_ErrorFunc errorfn; // 遇到可写事件时触发的回调函数
    int watch;
    void* ptr;
};

static WF_NIO *remainhead = NULL;
static int epollfd = 0;
static int wait_count;

int Init_Wf_Nio_Io () {
    if (epollfd > 0) {
        return 0;
    }
    if ((epollfd = epoll_create(1024)) < 0) { // epoll_create的参数在高版本中已经废弃了，填入一个大于0的数字都一样。
        perror("epoll fd create fail");
        printf("in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

int Wf_Add_Epoll_Fd (int fd, Wf_Nio_ReadFunc readfn, Wf_Nio_WriteFunc writefn, Wf_Nio_ErrorFunc errorfn, void *ptr) {
    int fdflags = fcntl(fd, F_GETFL, 0);
    if (fdflags < 0) {
        perror("get fd flags fail");
        printf("fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
        return -1;
    }
    if (fcntl(fd, F_SETFL, fdflags | O_NONBLOCK) < 0) {
        perror("set fd flags fail");
        printf("fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
        return -2;
    }
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
    WF_NIO *asyncio;
    if (remainhead == NULL) {
        if ((asyncio =  (WF_NIO*)malloc(sizeof (WF_NIO))) == NULL) {
            perror("malloc new WF_NIO obj fail");
            printf("in %s, at %d\n", __FILE__, __LINE__);
            return -4;
        }
    } else {
        asyncio = remainhead;
        remainhead = remainhead->ptr;
    }
    asyncio->fd = fd;
    asyncio->readfn = readfn;
    asyncio->writefn = writefn;
    asyncio->errorfn = errorfn;
    asyncio->watch = 0;
    asyncio->ptr = ptr;
    ev.data.ptr = asyncio;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev)) {
        perror("epoll ctl add fail");
        printf("fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
        asyncio->ptr = remainhead;
        remainhead = asyncio;
        return -5;
    }
    asyncio->watch = 1;
    return 0;
}

int Wf_Mod_Epoll_Fd (WF_NIO *asyncio, Wf_Nio_ReadFunc readfn, Wf_Nio_WriteFunc writefn, Wf_Nio_ErrorFunc errorfn, void *ptr) {
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
    if (epoll_ctl(epollfd, EPOLL_CTL_MOD, asyncio->fd, &ev)) {
        perror("epoll ctl mod fail");
        printf("fd:%d, in %s, at %d\n", asyncio->fd, __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

int Wf_Del_Epoll_Fd (WF_NIO *asyncio) {
    if (asyncio->watch && epoll_ctl(epollfd, EPOLL_CTL_DEL, asyncio->fd, NULL)) {
        perror("epoll ctl del fail");
        printf("fd:%d, in %s, at %d\n", asyncio->fd, __FILE__, __LINE__);
        return -1;
    }
    asyncio->watch = 0;
    asyncio->ptr = remainhead;
    remainhead = asyncio;
    return 0;
}

int Wf_Run_Event (int maxevents) {
    if (epollfd == 0) {
        printf("you need call Init_Wf_Nio_Io firstly, in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
    struct epoll_event *evs = (struct epoll_event*)malloc(maxevents * sizeof(struct epoll_event));
    if (evs == NULL) {
        perror("malloc epoll events fail");
        printf("in %s, at %d\n", __FILE__, __LINE__);
        return -2;
    }
LOOP:
    wait_count = epoll_wait(epollfd, evs, maxevents, -1);
    for (int i = 0 ; i < wait_count ; i++) {
        WF_NIO *asyncio = evs[i].data.ptr;
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

int Change_Socket_Opt (int fd, int keepalive, int keepidle, int keepintvl, int keepcnt) {
    unsigned int socksval = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (unsigned char*)&socksval, sizeof(socksval))) { // 关闭Nagle协议
        perror("setsockopt fail");
        printf("close Nagle protocol fail, fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
        return -1;
    }
    if (keepalive) {
        socksval = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (unsigned char*)&socksval, sizeof(socksval))) { // 启动tcp心跳包
            perror("setsockopt fail");
            printf("set socket keepalive fail, fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
            return -2;
        }
        socksval = keepidle;
        if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, (unsigned char*)&socksval, sizeof(socksval))) { // 设置tcp心跳包参数
            perror("setsockopt fail");
            printf("set socket keepidle fail, fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
            return -3;
        }
        socksval = keepintvl;
        if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, (unsigned char*)&socksval, sizeof(socksval))) { // 设置tcp心跳包参数
            perror("setsockopt fail");
            printf("set socket keepintvl fail, fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
            return -4;
        }
        socksval = keepcnt;
        if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, (unsigned char*)&socksval, sizeof(socksval))) { // 设置tcp心跳包参数
            perror("setsockopt fail");
            printf("set socket keepcnt fail, fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
            return -5;
        }
    }
    socklen_t socksval_len = sizeof(socksval);
    if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (unsigned char*)&socksval, &socksval_len)) {
        perror("getsockopt fail");
        printf("get send buffer fail, fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
        return -6;
    }
    // printf("old send buffer is %d, fd:%d\n", socksval, fd);
    socksval = MAXDATASIZE;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (unsigned char*)&socksval, sizeof (socksval))) {
        perror("setsockopt fail");
        printf("set send buffer fail, fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
        return -7;
    }
    socksval_len = sizeof(socksval);
    if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (unsigned char*)&socksval, &socksval_len)) {
        perror("getsockopt fail");
        printf("get send buffer fail, fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
        return -8;
    }
    // printf("new send buffer is %d, fd:%d\n", socksval, fd);
    // 修改接收缓冲区大小
    socksval_len = sizeof(socksval);
    if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (unsigned char*)&socksval, &socksval_len)) {
        perror("getsockopt fail");
        printf("get receive buffer fail, fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
        return -9;
    }
    // printf("old receive buffer is %d, fd:%d\n", socksval, fd);
    socksval = MAXDATASIZE;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&socksval, sizeof(socksval))) {
        perror("setsockopt fail");
        printf("set receive buffer fail, fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
        return -10;
    }
    socksval_len = sizeof(socksval);
    if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (unsigned char*)&socksval, &socksval_len)) {
        perror("getsockopt fail");
        printf("get receive buffer fail, fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
        return -11;
    }
    // printf("new receive buffer is %d, fd:%d\n", socksval, fd);
}
