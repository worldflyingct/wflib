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
    WF_NIO *asyncio;
    Wf_Http_Required_Handle requirehandle;
    void* ptr;
};
static WFHTTP *client_remainhead = NULL;

struct WFHTTPD {
    WF_NIO *asyncio;
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

int Http_Write_Finish (WF_NIO *asyncio, int fd, void *ptr) {
    printf("Http_Write_Finish, in %s, at %d\n", __FILE__, __LINE__);
    if (Wf_Del_Epoll_Fd(asyncio)) {
        printf("Wf_Del_Epoll_Fd error, in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
    close(fd);
    return 0;
}

int Http_Error_Finish (WF_NIO *asyncio, int fd, void *ptr, uint32_t events) {
    printf("Http_Error_Finish, in %s, at %d\n", __FILE__, __LINE__);
    if (Wf_Del_Epoll_Fd(asyncio)) {
        printf("Wf_Del_Epoll_Fd error, in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
    close(fd);
    return 0;
}

int Http_End (WFHTTP* wfhttp, unsigned char *data, unsigned int size) {
    static unsigned char buff[4096];
    unsigned int header_size = sprintf(buff, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-length: %d\r\nAccess-Control-Allow-Origin: *\r\n\r\n", size);
    memcpy(buff + header_size, data, size);
    Wf_Nio_Write_fd(wfhttp->asyncio, buff, header_size + size, Http_Write_Finish, Http_Error_Finish);
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
    wfhttp->asyncio = asyncio;
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
    wfhttp->requirehandle = wfhttpd->requirehandle;
    wfhttp->asyncio = NULL;
    wfhttp->ptr = wfhttpd->ptr;
    WF_NIO *new_asyncio = Wf_Add_Epoll_Fd(newfd, wfhttp);
    if (!new_asyncio) {
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
    WFHTTPD *wfhttpd;
    if (server_remainhead == NULL) {
        if ((wfhttpd =  (WFHTTPD*)malloc(sizeof (WFHTTPD))) == NULL) {
            perror("malloc new WFHTTPD obj fail");
            printf("in %s, at %d\n", __FILE__, __LINE__);
            return -1;
        }
    } else {
        wfhttpd = server_remainhead;
        server_remainhead = server_remainhead->ptr;
    }
    wfhttpd->requirehandle = requirehandle;
    wfhttpd->asyncio = NULL;
    wfhttpd->ptr = ptr;
    if (Wf_Nio_Accept_fd(port, max_connect, Accept_Http_Socket, NULL, wfhttpd)) {
        printf("Wf_Add_Read_Listen fail, port:%d, in %s, at %d\n", port, __FILE__, __LINE__);
        return -2;
    }
    return 0;
}
