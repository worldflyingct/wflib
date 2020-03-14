#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include "wfws.h"
#include "wfhttp.h"

static WFWS *remainhead = NULL;

int Wf_Nio_Error_Ws_Socket (WF_NIO *asyncio, int fd, void *ptr, uint32_t events) {
    Wf_Del_Epoll_Fd(asyncio);
}

int Wf_Nio_Write_Ws_Socket (WF_NIO *asyncio, int fd, void *ptr) {
}

int Register_Ws_Socket (WF_NIO *asyncio, int fd, void *ptr) {
    ssize_t len;
    HTTPPARAM httphead[100];
    memset(httphead, 0, sizeof(httphead));
    unsigned char readbuf[2*1024*1024];
    if ((len = read(fd, readbuf, sizeof(readbuf))) < 0) {
        if (errno != 11) {
            perror("ws socket register read err");
            Wf_Del_Epoll_Fd (asyncio);
        }
        return -1;
    }
    enum HTTPMETHOD method;
    unsigned char *path;
    enum HTTPVERSION version;
    unsigned int head_len = ParseHttpHeader(readbuf, len, &method, path, &version, httphead, 100);
    for (int i = 0 ; i < 100 && httphead[i].value != NULL ; i++) {
        printf("%s:%s\n", httphead[i].key, httphead[i].value);
    }
}

int Accept_Ws_Socket (WF_NIO *asyncio, int listenfd, void *ptr) {
    struct sockaddr_in sin;
    socklen_t in_addr_len = sizeof(struct sockaddr_in);
    int fd;
    if ((fd = accept(listenfd, (struct sockaddr*)&sin, &in_addr_len)) < 0) {
        perror("accept a new fd fail");
        printf("in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
    if (Change_Socket_Opt (fd, 0, 0, 0, 0)) {
        printf("in %s, at %d", __FILE__, __LINE__);
        close(fd);
        return -2;
    }
    if (Wf_Add_Epoll_Fd(fd, Register_Ws_Socket, NULL, Wf_Nio_Error_Ws_Socket, NULL)) {
        printf("in %s, at %d", __FILE__, __LINE__);
        close(fd);
        return -3;
    }
    return 0;
}

int Wf_Nio_Create_Ws_Server (unsigned short port, int max_connect) {
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
    if (Wf_Add_Epoll_Fd(fd, Accept_Ws_Socket, NULL, Wf_Nio_Error_Ws_Socket, NULL)) {
        printf("Wf_Add_Epoll_Fd fail, fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
        close(fd);
        return -4;
    }
    return 0;
}
