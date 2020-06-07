#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../wfasyncio/wfasyncio.h"
#include "../wfhttp/wfhttp.h"

enum STATUS {
    METHOD,
    IP,
    PORT,
    PATH,
    VERSION,
    PARAMKEY,
    PARAMVALUE
};

struct WFHTTP {
    Wf_Http_Required_Handle requirehandle;
    void* ptr;
};
static WFHTTP *client_remainhead = NULL;

struct WFHTTPD {
    Wf_Http_Required_Handle requirehandle;
    void* ptr;
};
typedef struct WFHTTPD WFHTTPD;
static WFHTTPD *server_remainhead = NULL;

unsigned int ParseHttpHeader (unsigned char* str,
                                unsigned int str_size,
                                enum HTTPMETHOD *method,
                                unsigned char **path,
                                unsigned int *path_len,
                                enum HTTPVERSION *version,
                                HTTPPARAM *httpparam,
                                unsigned int *httpparam_size) {
    unsigned int offset = 0;
    enum STATUS status = METHOD;
    int paramid;
    for (int i = 0 ; i < str_size ; i++) {
        switch (status) {
            case METHOD:
                if (str[i] == ' ') {
                    if (!memcmp(str, "GET", sizeof("GET") - 1)) {
                        *method = GET;
                    } else if (!memcmp(str, "POST", sizeof("POST") - 1)) {
                        *method = POST;
                    } else if (!memcmp(str, "PUSH", sizeof("PUSH") - 1)) {
                        *method = PUSH;
                    } else if (!memcmp(str, "DELETE", sizeof("DELETE") - 1)) {
                        *method = DELETE;
                    } else if (!memcmp(str, "CONNECT", sizeof("CONNECT") - 1)) {
                        *method = CONNECT;
                    } else {
                        printf("http method is unknown\n");
                        return 0;
                    }
                    status = PATH;
                    offset = i + 1;
                    *path = str + offset;
                }
                break;
            case PATH:
                if (str[i] == ' ') {
                    *path_len = i - offset;
                    str[i] = '\0';
                    offset = i + 1;
                    status = VERSION;
                }
                break;
            case VERSION:
                if (str[i] == '\r' && str[i+1] == '\n') {
                    if (!memcmp(str + offset, "HTTP/1.0", sizeof("HTTP/1.0") - 1)) {
                        *version = HTTP1_0;
                    } else if (!memcmp(str + offset, "HTTP/1.1", sizeof("HTTP/1.1") - 1)) {
                        *version = HTTP1_1;
                    } else if (!memcmp(str + offset, "HTTP/2.0", sizeof("HTTP/2.0") - 1)) {
                        *version = HTTP2_0;
                    } else {
                        printf("http version is unknown\n");
                        return 0;
                    }
                    i++;
                    offset = i + 1;
                    paramid = 0;
                    if (paramid >= *httpparam_size) {
                        return 0;
                    }
                    httpparam[paramid].key = str + offset;
                    status = PARAMKEY;
                }
                break;
            case PARAMKEY:
                if (str[i] == ':') {
                    httpparam[paramid].key_len = i - offset;
                    str[i] = '\0';
                    offset = i + 1;
                    httpparam[paramid].value = str + offset;
                    status = PARAMVALUE;
                } else if (str[i] == '\r' && str[i+1] == '\n') {
                    httpparam[paramid].value_len = i - offset;
                    *httpparam_size = paramid;
                    str[i] = '\0';
                    offset = i + 2;
                    return offset;
                }
                break;
            case PARAMVALUE:
                if (str[i] == '\r' && str[i+1] == '\n') {
                    httpparam[paramid].value_len = i - offset;
                    str[i] = '\0';
                    i++;
                    offset = i + 1;
                    paramid++;
                    if (paramid >= *httpparam_size) {
                        return 0;
                        break;
                    }
                    httpparam[paramid].key = str + offset;
                    status = PARAMKEY;
                } else if (str[i] == ' ') {
                    offset++;
                    httpparam[paramid].value = str + offset;
                }
                break;
        }
    }
    return 0;
}

int Receive_Http_Data (WF_NIO *asyncio, int fd, void *ptr, void* data, unsigned int size) {
    enum HTTPMETHOD httpmethod;
    unsigned char *path;
    unsigned int path_len;
    enum HTTPVERSION version;
    HTTPPARAM httpparam[1024];
    unsigned int httpparam_size = 1024;
    unsigned int httpheader_len = ParseHttpHeader(data, size, &httpmethod, &path, &path_len, &version, httpparam, &httpparam_size);
    WFHTTP *wfhttp = ptr;
    wfhttp->requirehandle(wfhttp, fd, wfhttp->ptr, data + httpheader_len, size - httpheader_len, httpmethod, path, path_len, version, httpparam, httpparam_size);
}

int Accept_Http_Socket (WF_NIO *asyncio, int fd, void *ptr, int newfd) {
    WFHTTP *wfhttp;
    if (client_remainhead == NULL) {
        if ((wfhttp =  (WFHTTP*)malloc(sizeof (WFHTTP))) == NULL) {
            perror("malloc new WFWS obj fail");
            printf("in %s, at %d\n", __FILE__, __LINE__);
            return -3;
        }
    } else {
        wfhttp = client_remainhead;
        client_remainhead = client_remainhead->ptr;
    }
    WFHTTPD *wfhttpd = ptr;
    wfhttp->ptr = wfhttpd->ptr;
    WF_NIO *new_asyncio = Wf_Add_Epoll_Fd(newfd, wfhttp);
    if (new_asyncio) {
        printf("Wf_Add_Epoll_Fd fail, fd:%d, in %s, at %d\n", newfd, __FILE__, __LINE__);
        close(newfd);
        return -4;
    }
    if (Wf_Nio_Read_fd(new_asyncio, Receive_Http_Data, NULL)) {
        printf("Wf_Add_Read_Listen fail, fd:%d, in %s, at %d\n", newfd, __FILE__, __LINE__);
        if (Wf_Del_Epoll_Fd(new_asyncio)) {
            printf("Wf_Del_Epoll_Fd fail, fd:%d, in %s, at %d\n", newfd, __FILE__, __LINE__);
        }
        close(newfd);
        return -5;
    }
    return 0;
}

int Wf_Nio_Create_Http_Server (unsigned short port, int max_connect, Wf_Http_Required_Handle requirehandle, void *ptr) {
    int fd = Wf_Nio_Listen_Port(port, max_connect);
    if (fd < 0) {
        return -1;
    }
    WFHTTPD *wfhttpd;
    if (server_remainhead == NULL) {
        if ((wfhttpd =  (WFHTTPD*)malloc(sizeof (WFHTTPD))) == NULL) {
            perror("malloc new WFHTTPD obj fail");
            printf("in %s, at %d\n", __FILE__, __LINE__);
            return -2;
        }
    } else {
        wfhttpd = server_remainhead;
        server_remainhead = server_remainhead->ptr;
    }
    wfhttpd->requirehandle = requirehandle;
    wfhttpd->ptr = ptr;
    WF_NIO *asyncio = Wf_Add_Epoll_Fd(fd, wfhttpd);
    if (asyncio) {
        printf("Wf_Add_Epoll_Fd fail, fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
        close(fd);
        return -3;
    }
    if (Wf_Nio_Accept_fd(asyncio, Accept_Http_Socket, NULL)) {
        printf("Wf_Add_Read_Listen fail, fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
        if (Wf_Del_Epoll_Fd (asyncio)) {
            printf("Wf_Del_Epoll_Fd fail, fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
        }
        close(fd);
        return -4;
    }
    return 0;
}
