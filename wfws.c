#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include "wfasyncio.h"
#include "wfhttp.h"
#include "sha1.h"
#include "base64.h"

#define magic_String      "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

typedef enum {
    UNREGSTER,
    REGSTER
} WSSTATUS;

typedef struct WFWS {
    WSSTATUS status;
    struct WFWS *ptr;
} WFWS;
static WFWS *remainhead = NULL;

int Wf_Nio_Error_Ws (WF_NIO *asyncio, int fd, void *ptr, uint32_t events) {
    WFWS *wfws = ptr;
    wfws->ptr = remainhead;
    remainhead = wfws;
    Wf_Del_Epoll_Fd(asyncio);
    return 0;
}

int Wf_Nio_Write_Ws_Socket (WF_NIO *asyncio, int fd, void *ptr) {
}

int Read_Ws_Socket (WF_NIO *asyncio, int fd, void *ptr) {
    WFWS *wfws = ptr;
    ssize_t len;
    unsigned char readbuf[2*1024*1024];
    if ((len = read(fd, readbuf, sizeof(readbuf))) < 0) {
        if (errno != 11) {
            perror("ws socket register read err");
            printf("in %s, at %d\n",  __FILE__, __LINE__);
            Wf_Nio_Error_Ws(asyncio, fd, ptr, 0);
        }
        return -1;
    }
    if (wfws->status == UNREGSTER) {
        HTTPPARAM httphead[100];
        memset(httphead, 0, sizeof(httphead));
        enum HTTPMETHOD method;
        unsigned char *path;
        unsigned int path_len;
        enum HTTPVERSION version;
        unsigned int head_len = ParseHttpHeader(readbuf, len, &method, &path, &path_len, &version, httphead, 100);
        int k = -1;
        for (int i = 0 ; i < 100 && httphead[i].value != NULL ; i++) {
            if (!memcmp(httphead[i].key, "Sec-WebSocket-Key", httphead[i].key_len)) {
                k = i;
            }
        }
        if (k == -1) {
            readbuf[len-1] = '\0';
            printf("readbuf:%s, in %s, at %d\n", readbuf,  __FILE__, __LINE__);fflush(stdout);
            Wf_Nio_Error_Ws(asyncio, fd, ptr, 0);
            return -2;
        }
        unsigned char input[64];
        unsigned char output[20];
        unsigned char base64[30];
        unsigned int base64_len = 30;
        unsigned char s[128];
        memcpy(input, httphead[k].value, httphead[k].value_len);
        memcpy(input + httphead[k].value_len, magic_String, sizeof(magic_String) - 1);
        SHA1Context sha;
        SHA1Reset(&sha);
        SHA1Input(&sha, input, httphead[k].value_len + sizeof(magic_String) - 1);
        SHA1Result(&sha, output);
        base64_encode(output, 20, base64, &base64_len);
        base64[base64_len] = '\0';
        int response_len = sprintf(s, "HTTP/1.1 101 Switching Protocols\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: Upgrade\r\n"
                    "Sec-WebSocket-Accept: %s\r\n"
                    "\r\n", base64);
        if (Wf_Nio_Write_fd (asyncio, s, response_len)) {
            Wf_Nio_Error_Ws(asyncio, fd, ptr, 0);
        }
        wfws->status == REGSTER;
    } else if (wfws->status == REGSTER) {
        printf("len:%d, in %s, at %d\n", len, __FILE__, __LINE__);
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
    WFWS *wfws;
    if (remainhead == NULL) {
        if ((wfws =  (WFWS*)malloc(sizeof (WFWS))) == NULL) {
            perror("malloc new WFWS obj fail");
            printf("in %s, at %d\n", __FILE__, __LINE__);
            return -3;
        }
    } else {
        wfws = remainhead;
        remainhead = remainhead->ptr;
    }
    wfws->status = UNREGSTER;
    if (Wf_Add_Epoll_Fd(fd, Read_Ws_Socket, NULL, Wf_Nio_Error_Ws, wfws)) {
        printf("in %s, at %d", __FILE__, __LINE__);
        close(fd);
        wfws->ptr = remainhead;
        remainhead = wfws;
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
    if (Wf_Add_Epoll_Fd(fd, Accept_Ws_Socket, NULL, NULL, NULL)) {
        printf("Wf_Add_Epoll_Fd fail, fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
        close(fd);
        return -4;
    }
    return 0;
}
