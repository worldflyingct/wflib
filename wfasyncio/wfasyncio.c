#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include "wfasyncio.h"

struct WF_NIO {
    int fd;
    Wf_Nio_AcceptFunc acceptfn;
    Wf_Nio_ReadFunc readfn;
    Wf_Nio_WriteFunc writefn;
    Wf_Nio_ErrorFunc errorfn;
    int readmode;
    int epollflags;
    unsigned char *data;
    int usesize;
    int fullsize;
    void *ptr;
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

int Set_Nio (int fd) {
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
    return 0;
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
    return 0;
}

int Wf_Del_All_Listen (WF_NIO *asyncio) {
    if (asyncio->epollflags) {
        if (epoll_ctl(epollfd, EPOLL_CTL_DEL, asyncio->fd, NULL)) {
            perror("epoll ctl del fail");
            printf("fd:%d, in %s, at %d\n", asyncio->fd, __FILE__, __LINE__);
            return -1;
        }
        asyncio->epollflags = 0;
        asyncio->acceptfn = NULL;
        asyncio->readfn = NULL;
        asyncio->writefn = NULL;
        asyncio->errorfn = NULL;
    }
    return 0;
}

int Wf_Del_Read_Listen (WF_NIO *asyncio) {
    if (asyncio->epollflags | EPOLLIN) {
        if (asyncio->epollflags | EPOLLOUT) { // 存在有写或接收监听
            int oldflags = asyncio->epollflags;
            oldflags &= ~EPOLLIN;
            struct epoll_event ev;
            ev.events = oldflags;
            ev.data.ptr = asyncio;
            if (epoll_ctl(epollfd, EPOLL_CTL_MOD, asyncio->fd, &ev)) {
                perror("epoll ctl mod fail");
                printf("fd:%d, in %s, at %d\n", asyncio->fd, __FILE__, __LINE__);
                return -1;
            }
            asyncio->epollflags = oldflags;
            asyncio->acceptfn = NULL;
            asyncio->readfn = NULL;
        } else if (Wf_Del_All_Listen(asyncio)) {
            return -2;
        }
    }
    return 0;
}

int Wf_Del_Write_Listen (WF_NIO *asyncio) {
    if (asyncio->epollflags | EPOLLOUT) {
        if (asyncio->epollflags | EPOLLIN) { // 存在有读监听
            int oldflags = asyncio->epollflags;
            oldflags &= ~EPOLLOUT;
            struct epoll_event ev;
            ev.events = oldflags;
            ev.data.ptr = asyncio;
            if (epoll_ctl(epollfd, EPOLL_CTL_MOD, asyncio->fd, &ev)) {
                perror("epoll ctl mod fail");
                printf("fd:%d, in %s, at %d\n", asyncio->fd, __FILE__, __LINE__);
                return -1;
            }
            asyncio->epollflags = oldflags;
            asyncio->writefn = NULL;
        } else if (Wf_Del_All_Listen(asyncio)) {
            return -2;
        }
    }
    return 0;
}

int Wf_Del_Epoll_Fd (WF_NIO *asyncio) {
    Wf_Del_All_Listen(asyncio);
    asyncio->ptr = remainhead;
    remainhead = asyncio;
}

WF_NIO *Wf_Add_Epoll_Fd (int fd, void *ptr) {
    if (Set_Nio(fd)) {
        perror("malloc new WF_NIO obj fail");
        printf("in %s, at %d\n", __FILE__, __LINE__);
        return NULL;
    }
    WF_NIO *asyncio;
    if (remainhead == NULL) {
        if ((asyncio =  (WF_NIO*)malloc(sizeof (WF_NIO))) == NULL) {
            perror("malloc new WF_NIO obj fail");
            printf("in %s, at %d\n", __FILE__, __LINE__);
            return NULL;
        }
        asyncio->data = NULL;
        asyncio->fullsize = 0;
    } else {
        asyncio = remainhead;
        remainhead = remainhead->ptr;
    }
    asyncio->fd = fd;
    asyncio->epollflags = 0;
    asyncio->acceptfn = NULL;
    asyncio->readfn = NULL;
    asyncio->writefn = NULL;
    asyncio->errorfn = NULL;
    asyncio->usesize = 0;
    asyncio->ptr = ptr;
    return asyncio;
}

int Wf_Nio_Listen_Port (unsigned short port, int max_connect) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("create socket fail");
        printf("in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET; // ipv4
    sin.sin_addr.s_addr = INADDR_ANY; // 本机任意ip
    sin.sin_port = htons(port);
    if (bind(fd, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        perror("bind port error");
        printf("port %d is cann't use, in %s, at %d\n", port, __FILE__, __LINE__);
        close(fd);
        return -2;
    }
    if (listen(fd, max_connect) < 0) {
        perror("bind port error");
        printf("listen port %d fail, in %s, at %d\n", port, __FILE__, __LINE__);
        close(fd);
        return -3;
    }
    return fd;
}

int Wf_Nio_Accept_fd (WF_NIO *asyncio, Wf_Nio_AcceptFunc acceptfn, Wf_Nio_ErrorFunc errorfn) {
    struct epoll_event ev;
    int oldflags = asyncio->epollflags;
    asyncio->epollflags |= EPOLLERR | EPOLLHUP | EPOLLRDHUP | EPOLLIN;
    asyncio->acceptfn = acceptfn;
    asyncio->readmode = 1;
    asyncio->errorfn = errorfn;
    ev.events = asyncio->epollflags;
    ev.data.ptr = asyncio;
    if (epoll_ctl(epollfd, oldflags ? EPOLL_CTL_MOD : EPOLL_CTL_ADD, asyncio->fd, &ev)) {
        perror("epoll ctl mod fail");
        printf("fd:%d, in %s, at %d\n", asyncio->fd, __FILE__, __LINE__);
        return -1;
    }
}

int Wf_Nio_Read_fd (WF_NIO *asyncio, Wf_Nio_ReadFunc readfn, Wf_Nio_ErrorFunc errorfn) {
    struct epoll_event ev;
    int oldflags = asyncio->epollflags;
    asyncio->epollflags |= EPOLLERR | EPOLLHUP | EPOLLRDHUP | EPOLLIN;
    asyncio->readfn = readfn;
    asyncio->readmode = 0;
    asyncio->errorfn = errorfn;
    ev.events = asyncio->epollflags;
    ev.data.ptr = asyncio;
    if (epoll_ctl(epollfd, oldflags ? EPOLL_CTL_MOD : EPOLL_CTL_ADD, asyncio->fd, &ev)) {
        perror("epoll ctl mod fail");
        printf("fd:%d, in %s, at %d\n", asyncio->fd, __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

int Wf_Nio_Write_fd (WF_NIO *asyncio, unsigned char *data, unsigned int size, Wf_Nio_WriteFunc writefn, Wf_Nio_ErrorFunc errorfn) {
    if (asyncio->epollflags | EPOLLOUT) {
        int remainsize = asyncio->usesize + size;
        if (asyncio->fullsize < remainsize) {
            unsigned char *tmpdata = (unsigned char*)malloc(remainsize);
            if (tmpdata == NULL) {
                perror("malloc fail");
                printf("remainsize:%d, in %s, at %d\n", remainsize,  __FILE__, __LINE__);
                return -1;
            }
            if (asyncio->usesize) {
                memcpy(tmpdata, asyncio->data, asyncio->usesize);
                free(asyncio->data);
            }
            asyncio->data = tmpdata;
            asyncio->fullsize = remainsize;
        }
        memcpy(asyncio->data + asyncio->usesize, data, size);
        asyncio->usesize = remainsize;
        if (asyncio->writefn != writefn) {
            if (asyncio->writefn) {
                printf("warning, write finish callback function changed, in %s, at %d\n",  __FILE__, __LINE__);
            }
            asyncio->writefn = writefn; // 替换写入完成回调。
        }
        if (asyncio->errorfn != errorfn) {
            if (asyncio->errorfn) {
                printf("warning, error finish callback function changed, in %s, at %d\n",  __FILE__, __LINE__);
            }
            asyncio->errorfn = errorfn; // 替换写入完成回调。
        }
    } else {
        ssize_t len = write(asyncio->fd, data, size);
        if (len == size) {
            if (writefn) {
                writefn(asyncio, asyncio->fd, asyncio->ptr);
            }
        } else if (len < 0) {
            if (errno != EAGAIN) {
                perror("write error");
                printf("fd:%d, errno:%d, in %s, at %d\n", asyncio->fd, errno,  __FILE__, __LINE__);
                return -2;
            }
            if (asyncio->fullsize < size) {
                unsigned char *tmpdata = (unsigned char*)malloc(size);
                if (tmpdata == NULL) {
                    perror("malloc fail");
                    printf("remainsize:%d, in %s, at %d\n", size,  __FILE__, __LINE__);
                    return -3;
                }
                if (asyncio->usesize) {
                    memcpy(tmpdata, asyncio->data, asyncio->usesize);
                    free(asyncio->data);
                }
                asyncio->data = tmpdata;
                asyncio->fullsize = size;
            }
            memcpy(asyncio->data + asyncio->usesize, data, size);
            asyncio->usesize = size;
            struct epoll_event ev;
            int oldflags = asyncio->epollflags;
            asyncio->epollflags |= EPOLLERR | EPOLLHUP | EPOLLRDHUP | EPOLLOUT;
            asyncio->writefn = writefn;
            asyncio->errorfn = errorfn;
            ev.events = asyncio->epollflags;
            ev.data.ptr = asyncio;
            if (epoll_ctl(epollfd, oldflags ? EPOLL_CTL_MOD : EPOLL_CTL_ADD, asyncio->fd, &ev)) {
                perror("epoll ctl mod fail");
                printf("fd:%d, in %s, at %d\n", asyncio->fd, __FILE__, __LINE__);
                return -4;
            }
        } else if (len < size) {
            printf("in %s, at %d\n",  __FILE__, __LINE__);
            int remainsize = size - len;
            if (asyncio->fullsize < remainsize) {
                unsigned char *tmpdata = (unsigned char*)malloc(remainsize);
                if (tmpdata == NULL) {
                    perror("malloc fail");
                    printf("remainsize:%d, in %s, at %d\n", remainsize,  __FILE__, __LINE__);
                    return -5;
                }
                if (asyncio->usesize) {
                    memcpy(tmpdata, asyncio->data, asyncio->usesize);
                    free(asyncio->data);
                }
                asyncio->data = tmpdata;
                asyncio->fullsize = remainsize;
            }
            memcpy(asyncio->data, data + len, remainsize);
            asyncio->usesize = remainsize;
            struct epoll_event ev;
            int oldflags = asyncio->epollflags;
            asyncio->epollflags |= EPOLLERR | EPOLLHUP | EPOLLRDHUP | EPOLLOUT;
            asyncio->writefn = writefn;
            asyncio->errorfn = errorfn;
            ev.events = asyncio->epollflags;
            ev.data.ptr = asyncio;
            if (epoll_ctl(epollfd, oldflags ? EPOLL_CTL_MOD : EPOLL_CTL_ADD, asyncio->fd, &ev)) {
                perror("epoll ctl mod fail");
                printf("fd:%d, in %s, at %d\n", asyncio->fd, __FILE__, __LINE__);
                return -6;
            }
        }
    }
    return 0;
}

int Wf_READ_Node (WF_NIO *asyncio) {
    if (asyncio->readmode) {
        static unsigned char data[MAXDATASIZE];
        ssize_t len = read(asyncio->fd, data, MAXDATASIZE);
        if (len >= 0) {
            asyncio->readfn(asyncio, asyncio->fd, asyncio->ptr, data, len);
        } else if (len < 0 && errno != EAGAIN) {
            perror("write error");
            printf("fd:%d, errno:%d, in %s, at %d\n", asyncio->fd, errno,  __FILE__, __LINE__);
            if (asyncio->errorfn) {
                asyncio->errorfn(asyncio, asyncio->fd, asyncio->ptr, 0);
            }
            return -1;
        }
    } else {
        struct sockaddr_in sin;
        socklen_t in_addr_len = sizeof(struct sockaddr_in);
        int newfd;
        if ((newfd = accept(asyncio->fd, (struct sockaddr*)&sin, &in_addr_len)) < 0) {
            perror("accept a new fd fail");
            printf("in %s, at %d\n", __FILE__, __LINE__);
            return -2;
        }
        if (Change_Socket_Opt(newfd, 1, 30, 3, 1)) {
            printf("in %s, at %d", __FILE__, __LINE__);
            close(newfd);
            return -3;
        }
        asyncio->acceptfn(asyncio, asyncio->fd, asyncio->ptr, newfd);
    }
    return 0;
}

int Wf_Write_Node (WF_NIO *asyncio) {
    ssize_t len = write(asyncio->fd, asyncio->data, asyncio->usesize);
    if (len == asyncio->usesize) {
        asyncio->usesize = 0;
        if (asyncio->writefn) {
            asyncio->writefn(asyncio, asyncio->fd, asyncio->ptr);
        }
    } else if (len < 0 && errno != EAGAIN) {
        perror("write error");
        printf("fd:%d, errno:%d, in %s, at %d\n", asyncio->fd, errno,  __FILE__, __LINE__);
        if (asyncio->errorfn) {
            asyncio->errorfn(asyncio, asyncio->fd, asyncio->ptr, 0);
        }
        return -1;
    } else if (len < asyncio->usesize) {
        int remainsize = asyncio->usesize - len;
        memcpy(asyncio->data, asyncio->data+len, remainsize);
        asyncio->usesize = remainsize;
    }
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
        if (!asyncio->epollflags) {
            ;
        } else if (events & ~(EPOLLIN | EPOLLOUT)) { // 除了读写外，还有别的事件
            if (asyncio->errorfn) {
                asyncio->errorfn(asyncio, asyncio->fd, asyncio->ptr, events);
            } else { // 处理错误的默认方法
                Wf_Del_Epoll_Fd(asyncio);
            }
        } else if (events & EPOLLIN) {
            Wf_READ_Node(asyncio);
        } else if (events & EPOLLOUT) {
            Wf_Write_Node(asyncio);
        }
    }
    goto LOOP;
}
