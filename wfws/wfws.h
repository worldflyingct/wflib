#ifndef __WF_WS_H__
#define __WF_WS_H__

typedef enum {
    TEXT,
    BLOB
} WS_DATA_TYPE;

typedef struct WFWS   WFWS;

typedef int (*Wf_Ws_Server_New_Client) (WFWS *wfws, void *ptr);
typedef int (*Wf_Ws_Server_Receive_Client) (WFWS *wfws, unsigned char *data, unsigned long size, WS_DATA_TYPE ws_data_type, void *ptr);
typedef int (*Wf_Ws_Server_Lose_Client) (WFWS *wfws, void *ptr);

int Send_Ws_Data (WFWS *wfws, unsigned char *data, unsigned long datasize, WS_DATA_TYPE ws_data_type);
int Wf_Nio_Create_Ws_Server (unsigned short port, int max_connect, Wf_Ws_Server_New_Client newfn, Wf_Ws_Server_Receive_Client receivefn, Wf_Ws_Server_Lose_Client losefn, void *ptr);

#endif
