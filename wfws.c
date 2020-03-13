#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include "wfws.h"

enum STATUS {
    UNREGISTER
};

struct WFWS {
    enum STATUS status; // 状态
    void* ptr;
};

static WFWS *remainhead = NULL;
int serverfd;

int Create_Ws_Server (unsigned short port, int max_connect) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("create socket fail");
        return -1;
    }
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET; // ipv4
    sin.sin_addr.s_addr = INADDR_ANY; // 本机任意ip
    sin.sin_port = htons(port);
    if (bind(fd, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        perror("bind port error");
        printf("port %d is can't use\n", port);
        close(fd);
        return -2;
    }
    if (listen(fd, max_connect) < 0) {
        perror("bind port error");
        printf("listen port %d fail\n", port, __FILE__, __LINE__);
        close(fd);
        return -3;
    }
    serverfd = fd;
    return fd;
}

int Read_Ws_Socket (WFASYNCIO *asyncio, int fd, void *ptr) {
}

int Write_Ws_Socket (WFASYNCIO *asyncio, int fd, void *ptr) {
}

int Error_Ws_Socket (WFASYNCIO *asyncio, int fd, void *ptr, uint32_t events) {
    WFWS *ws = ptr;
    ws->ptr = remainhead;
    remainhead = ws;
    Wf_Del_Epoll_Fd(asyncio);
}
