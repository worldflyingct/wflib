#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include "../wfasyncio/wfasyncio.h"
#include "../wfcoroutine/wfcoroutine.h"

#define MAXDATASIZE           2*1024*1024

static unsigned int maxsize = 0;
enum HOSTTYPE {
    IPv4,                     // ipv4地址
    IPv6,                     // ipv6地址
    DOMAIN                    // 域名地址
};

enum STATE {
    METHOD,
    IP,
    PORT,
    PARAMKEY,
    PARAMVALUE
};

unsigned int parsehttpproxyheader (char* oldheader, char* newheader, char* host, unsigned int* host_len, unsigned short* pport, int* isconnect, unsigned int* oldheader_len) {
    if (!(memcmp(oldheader, "GET ", sizeof("GET ") - 1) ||
            memcmp(oldheader, "POST ", sizeof("POST ") - 1) ||
            memcmp(oldheader, "PUSH ", sizeof("PUSH ") - 1) ||
            memcmp(oldheader, "DELETE ", sizeof("DELETE ") - 1) ||
            memcmp(oldheader, "CONNECT ", sizeof("CONNECT ") - 1))) {
        printf("http method is unknown\n");
        return 0;
    }
    unsigned short port;
    unsigned int offsetold = 0;
    unsigned int offsetnew = 0;
    enum STATE state = METHOD;
    for (int i = 0, str_len = strlen(oldheader) ; i < str_len ; i++) {
        switch (state) {
            case METHOD:
                if (oldheader[i] == ' ') {
                    i++;
                    if (i == 8) {
                        *isconnect = 1;
                        memcpy(newheader+offsetnew, oldheader+offsetnew, i);
                        offsetnew = i;
                        offsetold = i;
                    } else {
                        *isconnect = 0;
                        memcpy(newheader+offsetnew, oldheader+offsetnew, i);
                        offsetnew = i;
                        i += 7; // 头部一定为"http://"
                        offsetold = i;
                    }
                    state = IP;
                }
                break;
            case IP:
                if (oldheader[i] == ':' || oldheader[i] == '/') {
                    int len = i - offsetold;
                    memcpy(host, oldheader + offsetold, len);
                    host[len] = '\0';
                    *host_len = len;
                    if (oldheader[i] == '/') {
                        offsetold += len;
                        *pport = 80;
                        state = PARAMVALUE;
                    } else {
                        port = 0;
                        state = PORT;
                    }
                }
                break;
            case PORT:
                if (oldheader[i] == '/' || oldheader[i] == ' ') {
                    *pport = port;
                    state = PARAMVALUE;
                } else {
                    port = 10 * port + oldheader[i] - '0';
                }
                break;
            case PARAMKEY:
                if (oldheader[i] == ':') {
                    int len = i + 1 - offsetold;
                    if (!memcmp(oldheader + offsetold, "Proxy-Connection", 16)) {
                        memcpy(newheader + offsetnew, "Connection:", 11);
                        offsetnew += 11;
                        offsetold += len;
                    } else {
                        memcpy(newheader + offsetnew, oldheader + offsetold, len);
                        offsetnew += len;
                        offsetold += len;
                    }
                    state = PARAMVALUE;
                } else if (oldheader[i] == '\r' && oldheader[i+1] == '\n') {
                    i++;
                    int len = i+1-offsetold;
                    memcpy(newheader+offsetnew, oldheader+offsetold, len);
                    offsetnew += len;
                    offsetold += len;
                    *oldheader_len = offsetold;
                    newheader[offsetnew] = '\0';
                    str_len = 0;
                }
                break;
            case PARAMVALUE:
                if (oldheader[i] == '\r' && oldheader[i+1] == '\n') {
                    i++;
                    int len = i+1-offsetold;
                    memcpy(newheader+offsetnew, oldheader+offsetold, len);
                    offsetnew += len;
                    offsetold += len;
                    state = PARAMKEY;
                }
                break;
        }
    }
    return offsetnew;
}

int connect_ipv4 (unsigned char *addr, unsigned short port) {
    int targetfd;
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    memcpy(&sin.sin_addr.s_addr, addr, 4);
    if ((targetfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("create target fd fail, in %s, at %d\n", __FILE__, __LINE__);
        perror("err");
        return -1;
    }
    if (Wf_SET_NIO(targetfd) || Change_Socket_Opt(targetfd, 1, 20, 5, 3)) {
        printf("change socket buffer fail, fd:%d, in %s, at %d\n", targetfd, __FILE__, __LINE__);
        close(targetfd);
        return -2;
    }
    connect(targetfd, (struct sockaddr*)&sin, sizeof(sin));
    return targetfd;
}

int connect_ipv6 (unsigned char *addr, unsigned short port) {
    int targetfd;
    struct sockaddr_in6 sin6;
    memset(&sin6, 0, sizeof(struct sockaddr_in6));
    sin6.sin6_family = AF_INET6;
    sin6.sin6_port = htons(port);
    memcpy(sin6.sin6_addr.s6_addr, addr, 16);
    if ((targetfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
        printf("create target fd fail, in %s, at %d\n", __FILE__, __LINE__);
        perror("err");
        return -1;
    }
    if (Wf_SET_NIO(targetfd) || Change_Socket_Opt(targetfd, 1, 20, 5, 3)) {
        printf("change socket buffer fail, fd:%d, in %s, at %d\n", targetfd, __FILE__, __LINE__);
        close(targetfd);
        return -2;
    }
    connect(targetfd, (struct sockaddr*)&sin6, sizeof(sin6));
    return targetfd;
}

int Connect_Client (enum HOSTTYPE type, unsigned char *host, unsigned short port, int host_len) {
    int targetfd;
    if (type == DOMAIN) { // 域名
        unsigned char domain[256];
        memcpy(domain, host, host_len);
        domain[host_len] = '\0';
        struct hostent *ip = gethostbyname(domain); // 域名dns解析
        if(ip == NULL) {
            printf("get ip by domain error, domain:%s, in %s, at %d\n", domain,  __FILE__, __LINE__);
            return -1;
        }
        printf("target domain:%s, port:%d, in %s, at %d\n", domain, port, __FILE__, __LINE__);
        unsigned char *addr = ip->h_addr_list[0];
        if (ip->h_addrtype == AF_INET) { // ipv4
            if ((targetfd = connect_ipv4(addr, port)) < 0) {
                return -2;
            }
        } else if (ip->h_addrtype == AF_INET6) { // ipv6
            if ((targetfd = connect_ipv6(addr, port)) < 0) {
                return -3;
            }
        }
    } else if (type == IPv4) { // ipv4
        printf("target ipv4:%d.%d.%d.%d, in %s, at %d\n", host[0], host[1], host[2], host[3], __FILE__, __LINE__);
        if ((targetfd = connect_ipv4(host, port)) < 0) {
            return -4;
        }
    } else if (type == IPv6) { // ipv6
        printf("target ipv6:%x%x:%x%x:%x%x:%x%x:%x%x:%x%x:%x%x:%x%x, in %s, at %d\n",
            host[0], host[1], host[2], host[3], host[4], host[5], host[6], host[7],
            host[8], host[9], host[10], host[11], host[12], host[13], host[14], host[15],
            __FILE__, __LINE__);
        if ((targetfd = connect_ipv6(host, port)) < 0) {
            return -5;
        }
    }
    return targetfd;
}

void Wf_Io_Copy (WF_CTX* wf_ctx, void* ptr) {
    WF_CTX* clientctx = ptr;
    if (wf_ctx->status == ERROR) {
        printf("connect lose, in %s, at %d\n",  __FILE__, __LINE__);
        Delete_Coroutine(wf_ctx);
        Delete_Coroutine(clientctx);
        return;
    }
    unsigned char readbuf[MAXDATASIZE];
    while (1) {
        ssize_t len;
        while (1) {
            len = Coroutine_ReadData(wf_ctx, readbuf, sizeof(readbuf));
            if (len < 0) {
                if (errno != 11) {
                    perror("err");
                    printf("http proxy read data fail, in %s, at %d\n", __FILE__, __LINE__);
                    Delete_Coroutine(wf_ctx);
                    Delete_Coroutine(clientctx);
                    return;
                }
                Suspend_Coroutine(wf_ctx);
                if (wf_ctx->status == ERROR) {
                    printf("connect lose, in %s, at %d\n",  __FILE__, __LINE__);
                    Delete_Coroutine(wf_ctx);
                    Delete_Coroutine(clientctx);
                    return;
                }
                continue;
            }
            if (maxsize < len) {
                maxsize = len;
                printf("maxsize: %d, in %s, at %d\n", maxsize, __FILE__, __LINE__);
            }
            break;
        }
        if (Coroutine_WriteData(clientctx, readbuf, len)) {
            printf("http proxy write data fail, in %s, at %d\n", __FILE__, __LINE__);
            Delete_Coroutine(wf_ctx);
            Delete_Coroutine(clientctx);
            return;
        }
        Suspend_Coroutine(wf_ctx);
        if (wf_ctx->status == ERROR) {
            printf("connect lose, in %s, at %d\n",  __FILE__, __LINE__);
            Delete_Coroutine(wf_ctx);
            Delete_Coroutine(clientctx);
            return;
        }
    }
}

void Wf_Http_Proxy_Coroutine (WF_CTX *wf_ctx, void *ptr) {
    if (wf_ctx->status == ERROR) {
        printf("connect lose, in %s, at %d\n",  __FILE__, __LINE__);
        Delete_Coroutine(wf_ctx);
        return;
    }
    unsigned char readbuf[MAXDATASIZE];
    ssize_t len;
    while (1) {
        len = Coroutine_ReadData(wf_ctx, readbuf, sizeof(readbuf));
        if (len < 0) {
            if (errno != 11) {
                printf("read error, errno:%d, in %s, at %d\n", errno,  __FILE__, __LINE__);
                perror("err");
                Delete_Coroutine(wf_ctx);
                return;
            }
            Suspend_Coroutine(wf_ctx);
            if (wf_ctx->status == ERROR) {
                printf("connect lose, in %s, at %d\n",  __FILE__, __LINE__);
                Delete_Coroutine(wf_ctx);
                return;
            }
            continue;
        }
        if (maxsize < len) {
            maxsize = len;
            printf("maxsize: %d, in %s, at %d\n", maxsize, __FILE__, __LINE__);
        }
        break;
    }
    printf("len:%d\n", len);
    unsigned char newheader[1024];
    unsigned char host[256];
    unsigned short port = 0;
    unsigned int host_len = 0;
    int isconnect = 0;
    unsigned int oldheader_len = 0;
    unsigned int newheader_len = parsehttpproxyheader(readbuf, newheader, host, &host_len, &port, &isconnect, &oldheader_len);
    if (newheader_len == 0 || oldheader_len == 0 || host_len == 0 || port == 0 || oldheader_len > 5*1024 || len < oldheader_len) {
        readbuf[len-1] = '\0';
        printf("newheader_len:%d\n", newheader_len);
        printf("oldheader_len:%d\n", oldheader_len);
        printf("host_len:%d\n", host_len);
        printf("port:%d\n", port);
        printf("parse http proxy header error, oldheader:%s, in %s, at %d\n", readbuf,  __FILE__, __LINE__);
        Delete_Coroutine(wf_ctx);
        return;
    }
    int targetfd = Connect_Client(DOMAIN, host, port, host_len);
    if (targetfd < 0) {
        printf("connect client error, in %s, at %d\n",  __FILE__, __LINE__);
        Delete_Coroutine(wf_ctx);
        return;
    }
    WF_CTX* serverctx = Coroutine_Wait_Event(wf_ctx, targetfd, 0, 1);
    if (serverctx == NULL) {
        printf("coroutine wait event fail, in %s, at %d\n",  __FILE__, __LINE__);
        Delete_Coroutine(wf_ctx);
        return;
    }
    if (isconnect == 1) { // connect模式
        unsigned char data[] = "HTTP/1.1 200 Connection established\r\n\r\n";
        if (Coroutine_WriteData(wf_ctx, data, sizeof(data)-1)) {
            printf("http proxy write data fail, in %s, at %d\n", __FILE__, __LINE__);
            Delete_Coroutine(wf_ctx);
            Delete_Coroutine(serverctx);
            return;
        }
    } else { // http模式
        unsigned char writebuf[MAXDATASIZE];
        printf("newheader_len: %d, len - oldheader_len: %d, in %s, at %d\n", newheader_len, len - oldheader_len,  __FILE__, __LINE__);
        memcpy(writebuf, newheader, newheader_len);
        if (oldheader_len > len) {
            memcpy(writebuf + newheader_len, readbuf + oldheader_len, len - oldheader_len);
        }
        if (Coroutine_WriteData(serverctx, writebuf, len + newheader_len - oldheader_len)) {
            printf("http proxy write data fail, in %s, at %d\n", __FILE__, __LINE__);
            Delete_Coroutine(wf_ctx);
            Delete_Coroutine(serverctx);
            return;
        }
    }
    if (Wf_Mod_Coroutine(serverctx, (void*)Wf_Io_Copy, 0, wf_ctx)) {
        printf("modify coroutine fail, in %s, at %d\n", __FILE__, __LINE__);
        Delete_Coroutine(wf_ctx);
        Delete_Coroutine(serverctx);
        return;
    }
    while (1) { // 客户端到服务端
        while (1) {
            Suspend_Coroutine(wf_ctx);
            if (wf_ctx->status == ERROR) {
                printf("connect lose, in %s, at %d\n",  __FILE__, __LINE__);
                Delete_Coroutine(wf_ctx);
                Delete_Coroutine(serverctx);
                return;
            }
            len = Coroutine_ReadData(wf_ctx, readbuf, sizeof(readbuf));
            if (len < 0) {
                if (errno != 11) {
                    printf("read error, errno:%d, in %s, at %d\n", errno,  __FILE__, __LINE__);
                    perror("err");
                    Delete_Coroutine(wf_ctx);
                    Delete_Coroutine(serverctx);
                    return;
                }
                perror("err");
                continue;
            }
            if (maxsize < len) {
                maxsize = len;
                printf("maxsize: %d, in %s, at %d\n", maxsize, __FILE__, __LINE__);
            }
            break;
        }
        if (Coroutine_WriteData(serverctx, readbuf, len)) {
            printf("http proxy write data fail, in %s, at %d\n", __FILE__, __LINE__);
            Delete_Coroutine(wf_ctx);
            Delete_Coroutine(serverctx);
            return;
        }
    }
}

int Accept_Socks5_Socket (WF_NIO *asyncio, int listenfd, void *ptr) {
}

int Accept_Http_Socket (WF_NIO *asyncio, int listenfd, void *ptr) {
    struct sockaddr_in sin;
    socklen_t in_addr_len = sizeof(struct sockaddr_in);
    int fd = accept(listenfd, (struct sockaddr*)&sin, &in_addr_len);
    if (fd < 0) {
        printf("accept a new fd fail, fd:%d, in %s, at %d\n", fd,  __FILE__, __LINE__);
        return -1;
    }
    if (Wf_SET_NIO (fd) || Change_Socket_Opt(fd, 1, 20, 5, 3)) {
        printf("in %s, at %d", __FILE__, __LINE__);
        close(fd);
        return -2;
    }
    if (Wf_Create_Coroutine (Wf_Http_Proxy_Coroutine, fd, 0, NULL)) {
        printf("in %s, at %d", __FILE__, __LINE__);
        close(fd);
        return -3;
    }
    return 0;
}

int Create_Socket_Fd (unsigned short port, unsigned int maxconnect, Wf_Nio_ReadFunc readfn) {
    if (port == 0) {
        printf("http proxy is disabled, in %s, at %d\n", __FILE__, __LINE__);
        return 0;
    }
    struct sockaddr_in sin;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("run socket function is fail, fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
        return -1;
    }
    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET; // ipv4
    sin.sin_addr.s_addr = INADDR_ANY; // 本机任意ip
    sin.sin_port = htons(port);
    if (bind(fd, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        printf("bind proxy port %d fail, fd:%d, in %s, at %d\n", port, fd, __FILE__, __LINE__);
        close(fd);
        return -2;
    }
    if (listen(fd, maxconnect) < 0) {
        printf("listen proxy port %d fail, fd:%d, in %s, at %d\n", port, fd, __FILE__, __LINE__);
        close(fd);
        return -3;
    }
    if (Wf_SET_NIO(fd) || Wf_Add_Epoll_Fd(fd, readfn, NULL, NULL, NULL)) {
        printf("Wf_Add_Epoll_Fd fail, fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
        close(fd);
        return -4;
    }
    return 0;
}

int Init_Proxy (unsigned short httpport, unsigned short socks5port, unsigned int maxconnect) {
    Create_Socket_Fd(httpport, maxconnect, Accept_Http_Socket);
    // Create_Socket_Fd(socks5port, maxconnect, Accept_Socks5_Socket);
    return 0;
}
