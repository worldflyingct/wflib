#ifndef __WFHTTP_H__
#define __WFHTTP_H__

enum HTTPMETHOD {
    GET,
    POST,
    PUSH,
    DELETE,
    CONNECT,
    UNKNOWN
};

enum HTTPVERSION {
    HTTP1_0,
    HTTP1_1,
    HTTP2_0
};

typedef struct WFHTTP WFHTTP;

typedef struct HTTPPARAM {
    unsigned char *key;
    unsigned int key_len;
    unsigned char *value;
    unsigned int value_len;
} HTTPPARAM;

typedef int (*Wf_Http_Required_Handle) (WFHTTP *wfhttp, int fd, void* ptr, unsigned char *body, unsigned int body_size, enum HTTPMETHOD httpmethod, unsigned char *path, unsigned int path_len, enum HTTPVERSION version, HTTPPARAM *httpparam, unsigned int httpparam_size);

unsigned int ParseHttpHeader (unsigned char* str,
                                unsigned int str_size,
                                enum HTTPMETHOD *method,
                                unsigned char **path,
                                unsigned int *path_len,
                                enum HTTPVERSION *version,
                                HTTPPARAM *httpparam,
                                unsigned int *httpparam_size);
int Receive_Http_Data (WF_NIO *asyncio, int fd, void *ptr, void* data, unsigned int size);
int Accept_Http_Socket (WF_NIO *asyncio, int fd, void *ptr, int newfd);
int Wf_Nio_Create_Http_Server (unsigned short port, int max_connect, Wf_Http_Required_Handle requirehandle, void *ptr);

#endif
