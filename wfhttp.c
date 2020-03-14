#include <stdio.h>
#include <string.h>
#include "wfhttp.h"

enum STATUS {
    METHOD,
    IP,
    PORT,
    PATH,
    VERSION,
    PARAMKEY,
    PARAMVALUE
};

unsigned int ParseHttpHeader (unsigned char* str,
                                unsigned int str_size,
                                enum HTTPMETHOD *method,
                                unsigned char **path,
                                unsigned int *path_len,
                                enum HTTPVERSION *version,
                                HTTPPARAM *httpparam,
                                unsigned int httpparam_size) {
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
                        return -1;
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
                        return -2;
                    }
                    i++;
                    offset = i + 1;
                    paramid = 0;
                    if (paramid >= httpparam_size) {
                        return -3;
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
                    if (paramid >= httpparam_size) {
                        return -4;
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
