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

typedef struct HTTPPARAM {
    unsigned char *key;
    unsigned int key_len;
    unsigned char *value;
    unsigned int value_len;
} HTTPPARAM;

unsigned int ParseHttpHeader (unsigned char* str,
                                unsigned int str_size,
                                enum HTTPMETHOD *method,
                                unsigned char **path,
                                unsigned int *path_len,
                                enum HTTPVERSION *version,
                                HTTPPARAM *httpparam,
                                unsigned int httpparam_size);

#endif
