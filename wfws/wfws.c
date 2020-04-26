#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include "wfws.h"
#include "../wfasyncio/wfasyncio.h"
#include "../wfcoroutine/wfcoroutine.h"
#include "../wfhttp/wfhttp.h"
#include "../wfcrypto/sha1.h"
#include "../wfcrypto/base64.h"

#define magic_String      "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

typedef struct {
    Wf_Ws_Server_New_Client newfn;
    Wf_Ws_Server_Receive_Client receivefn;
    Wf_Ws_Server_Lose_Client losefn;
    void *ptr;
} WFWSSERVER;
WFWSSERVER *server_remainhead = NULL;

typedef struct {
    unsigned char *data;
    unsigned int size;
    void *tail;
} WS_DATA;
static WS_DATA *data_remainhead = NULL;

struct WFWS {
    Wf_Ws_Server_New_Client newfn;
    Wf_Ws_Server_Receive_Client receivefn;
    Wf_Ws_Server_Lose_Client losefn;
    WF_CTX* ctx;
    void *ptr;
};
WFWS *client_remainhead = NULL;

unsigned long Parse_Ws_Data (unsigned char *buff, unsigned long buffsize, unsigned char **resdata) {
    unsigned char *mask;
    unsigned char *data;
    int hasmask = buff[1] & 0x80;
    unsigned long data_len = buff[1] & 0x7f;
    if (data_len < 126) {
        if (buffsize < data_len) {
            return -1;
        }
        if (hasmask) {
            mask = buff + 2;
            data = mask + 4;
            for (unsigned long i = 0 ; i < data_len ; i++) {
                data[i] ^= mask[i & 0x03];
            }
        } else {
            data = buff + 2;
        }
    } else if (data_len == 126) {
        data_len = ((unsigned long)buff[2] << 8) | (unsigned long)buff[3];
        if (buffsize < data_len) {
            return -1;
        }
        if (hasmask) {
            mask = buff + 4;
            data = mask + 4;
            for (unsigned long i = 0 ; i < data_len ; i++) {
                data[i] ^= mask[i & 0x03];
            }
        } else {
            data = buff + 4;
        }
    } else {
        data_len = ((unsigned long)buff[2] << 56) | ((unsigned long)buff[3] << 48) | ((unsigned long)buff[4] << 40) | ((unsigned long)buff[5] << 32) | ((unsigned long)buff[6] << 24) | ((unsigned long)buff[7] << 16) | ((unsigned long)buff[8] << 8) | (unsigned long)buff[9];
        if (buffsize < data_len) {
            return -1;
        }
        if (hasmask) {
            mask = buff + 10;
            data = mask + 4;
            for (unsigned long i = 0 ; i < data_len ; i++) {
                data[i] ^= mask[i & 0x03];
            }
        } else {
            data = buff + 10;
        }
    }
    *resdata = data;
    return data_len;
}

int Wf_Nio_Error_Ws (void *ptr, uint32_t events) {
    printf("events:%d, in %s, at %d\n", events, __FILE__, __LINE__);
    WFWS *wfws = ptr;
    if (wfws->losefn) {
        wfws->losefn(wfws, wfws->ptr);
    }
    wfws->ptr = client_remainhead;
    client_remainhead = wfws;
    if (Delete_Coroutine(wfws->ctx)) {
        printf("delete coroutine in %s, at %d\n", __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

int Send_Ws_Data (WFWS *wfws, unsigned char *data, unsigned long datasize, WS_DATA_TYPE ws_data_type) {
    if (datasize < 126) {
        unsigned char senddata[datasize+2];
        senddata[0] = ws_data_type == TEXT ? 0x81 : 0x82;
        senddata[1] = datasize;
        memcpy(senddata + 2, data, datasize);
        if (Coroutine_WriteData (wfws->ctx, senddata, datasize + 2)) {
            printf("in %s, at %d\n", __FILE__, __LINE__);
            Wf_Nio_Error_Ws(wfws, 0);
            return -1;
        }
    } else if (datasize < 65536) {
        unsigned char senddata[datasize+4];
        senddata[0] = ws_data_type == TEXT ? 0x81 : 0x82;
        senddata[1] = 126;
        senddata[2] = (datasize >> 8) & 0xff;
        senddata[3] = datasize & 0xff;
        memcpy(senddata + 4, data, datasize);
        if (Coroutine_WriteData (wfws->ctx, senddata, datasize + 4)) {
            printf("in %s, at %d\n", __FILE__, __LINE__);
            Wf_Nio_Error_Ws(wfws, 0);
            return -2;
        }
    } else {
        unsigned char senddata[datasize+10];
        senddata[0] = ws_data_type == TEXT ? 0x81 : 0x82;
        senddata[1] = 127;
        senddata[2] = (datasize >> 56) & 0xff;
        senddata[3] = (datasize >> 48) & 0xff;
        senddata[4] = (datasize >> 40) & 0xff;
        senddata[5] = (datasize >> 32) & 0xff;
        senddata[6] = (datasize >> 24) & 0xff;
        senddata[7] = (datasize >> 16) & 0xff;
        senddata[8] = (datasize >> 8) & 0xff;
        senddata[9] = datasize & 0xff;
        memcpy(senddata + 10, data, datasize);
        if (Coroutine_WriteData (wfws->ctx, senddata, datasize + 4)) {
            printf("in %s, at %d\n", __FILE__, __LINE__);
            Wf_Nio_Error_Ws(wfws, 0);
            return -3;
        }
    }
    return 0;
}

void Wf_Ws_Coroutine (WF_CTX *wf_ctx, CORO_STATUS status, void *ptr) {
    WFWS *wfws = ptr;
    if (status == ERROR) {
        printf("ws coroutine err, in %s, at %d\n", __FILE__, __LINE__);
        Wf_Nio_Error_Ws(wfws, 0);
        return;
    }
    ssize_t len;
    unsigned char readbuf[2*1024*1024];
    if ((len = Coroutine_ReadData(wf_ctx, readbuf, sizeof(readbuf))) < 0) {
        if (errno != 11) {
            perror("ws socket register read err");
            printf("in %s, at %d\n",  __FILE__, __LINE__);
            Wf_Nio_Error_Ws(wfws, 0);
        }
        return;
    }
    HTTPPARAM httphead[100];
    memset(httphead, 0, sizeof(httphead));
    enum HTTPMETHOD method;
    unsigned char *path;
    unsigned int path_len;
    enum HTTPVERSION version;
    unsigned int head_len = ParseHttpHeader(readbuf, len, &method, &path, &path_len, &version, httphead, 100);
    if (head_len == -1) {
        readbuf[len-1] = '\0';
        printf("receive_data: %s, in %s, at %d\n", readbuf, __FILE__, __LINE__);
        printf("in %s, at %d\n", __FILE__, __LINE__);
        Wf_Nio_Error_Ws(wfws, 0);
        return;
    }
    int k = -1;
    for (int i = 0 ; i < 100 && httphead[i].value != NULL ; i++) {
        if (!memcmp(httphead[i].key, "Sec-WebSocket-Key", httphead[i].key_len)) {
            k = i;
        }
    }
    if (k == -1) {
        readbuf[len-1] = '\0';
        printf("receive_data: %s, in %s, at %d\n", readbuf, __FILE__, __LINE__);
        Wf_Nio_Error_Ws(wfws, 0);
        return;
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
    if (Coroutine_WriteData (wf_ctx, s, response_len)) {
        printf("in %s, at %d\n", __FILE__, __LINE__);
        Wf_Nio_Error_Ws(wfws, 0);
    }
    wfws->ctx = wf_ctx;
    wfws->newfn(wfws, wfws->ptr);
    while (1) {
        Suspend_Coroutine(wf_ctx);
        if (status == ERROR) {
            printf("ws coroutine err, in %s, at %d\n", __FILE__, __LINE__);
            Wf_Nio_Error_Ws(wfws, 0);
        }
        if ((len = Coroutine_ReadData(wf_ctx, readbuf, sizeof(readbuf))) < 0) {
            if (errno != 11) {
                perror("ws socket register read err");
                printf("in %s, at %d\n",  __FILE__, __LINE__);
                Wf_Nio_Error_Ws(wfws, 0);
            }
            continue;
        }
        if (readbuf[0] & 0x80) {
            unsigned char opcode = readbuf[0] & 0x0f;
            if (opcode) { // 唯一一帧
                if (opcode == 0x01) {
                    unsigned char *data;
                    unsigned long datasize;
                    if (!(datasize = Parse_Ws_Data(readbuf, len, &data)) == -1) {
                        Wf_Nio_Error_Ws(wfws, 0);
                    }
                    data[datasize] = '\0';
                    wfws->receivefn(wfws, data, datasize, TEXT, wfws->ptr);
                } else if (opcode == 0x02) {
                    unsigned char *data;
                    unsigned long datasize;
                    if ((datasize = Parse_Ws_Data(readbuf, len, &data)) == -1) {
                        Wf_Nio_Error_Ws(wfws, 0);
                    }
                    wfws->receivefn(wfws, data, datasize, BLOB, wfws->ptr);
                } else if (opcode == 0x08) { // 断开帧
                    Wf_Nio_Error_Ws(wfws, 0);
                }
            } else { // 连续帧的最后一帧
            }
        } else { // 连续帧的中间帧
        }
    }
}

int Accept_Ws_Socket (WF_NIO *asyncio, int listenfd, void *ptr) {
    WFWSSERVER *wfwsserver = ptr;
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
    if (client_remainhead == NULL) {
        if ((wfws =  (WFWS*)malloc(sizeof (WFWS))) == NULL) {
            perror("malloc new WFWS obj fail");
            printf("in %s, at %d\n", __FILE__, __LINE__);
            return -3;
        }
    } else {
        wfws = client_remainhead;
        client_remainhead = client_remainhead->ptr;
    }
    wfws->newfn = wfwsserver->newfn;
    wfws->receivefn = wfwsserver->receivefn;
    wfws->losefn = wfwsserver->losefn;
    wfws->ctx = NULL;
    wfws->ptr = wfwsserver->ptr;
    if (Wf_Create_Coroutine (Wf_Ws_Coroutine, fd, 0, wfws)) {
        printf("in %s, at %d", __FILE__, __LINE__);
        close(fd);
        wfws->ptr = client_remainhead;
        client_remainhead = wfws;
        return -3;
    }
    return 0;
}

int Wf_Nio_Create_Ws_Server (unsigned short port, int max_connect, Wf_Ws_Server_New_Client newfn, Wf_Ws_Server_Receive_Client receivefn, Wf_Ws_Server_Lose_Client losefn, void *ptr) {
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
    WFWSSERVER *wfwsserver;
    if (server_remainhead == NULL) {
        if ((wfwsserver =  (WFWSSERVER*)malloc(sizeof (WFWSSERVER))) == NULL) {
            perror("malloc new WFWSSERVER obj fail");
            printf("in %s, at %d\n", __FILE__, __LINE__);
            return -3;
        }
    } else {
        wfwsserver = server_remainhead;
        server_remainhead = server_remainhead->ptr;
    }
    wfwsserver->newfn = newfn;
    wfwsserver->receivefn = receivefn;
    wfwsserver->losefn = losefn;
    wfwsserver->ptr = ptr;
    if (Wf_Add_Epoll_Fd(fd, Accept_Ws_Socket, NULL, NULL, wfwsserver)) {
        printf("Wf_Add_Epoll_Fd fail, fd:%d, in %s, at %d\n", fd, __FILE__, __LINE__);
        close(fd);
        return -4;
    }
    return 0;
}
