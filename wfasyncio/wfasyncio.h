#ifndef __WF_ASYNC_IO_H__
#define __WF_ASYNC_IO_H__

#include <sys/epoll.h>

#define MAXDATASIZE   2*1024*1024

typedef struct WF_NIO WF_NIO;

typedef int (*Wf_Nio_AcceptFunc) (WF_NIO *asyncio, int fd, void *ptr, int newfd);
typedef int (*Wf_Nio_ReadFunc) (WF_NIO *asyncio, int fd, void *ptr, void* data, unsigned int size);
typedef int (*Wf_Nio_WriteFunc) (WF_NIO *asyncio, int fd, void *ptr);
typedef int (*Wf_Nio_ErrorFunc) (WF_NIO *asyncio, int fd, void *ptr, uint32_t events);

int Init_Wf_Nio_Io ();
WF_NIO *Wf_Add_Epoll_Fd (int fd, void *ptr);
int Wf_Del_Epoll_Fd (WF_NIO *asyncio);
int Wf_Del_Write_Listen (WF_NIO *asyncio);
int Wf_Del_Read_Listen (WF_NIO *asyncio);
int Wf_Del_All_Listen (WF_NIO *asyncio);
int Wf_Nio_Write_fd (WF_NIO *asyncio, unsigned char *data, unsigned int size, Wf_Nio_WriteFunc writefn, Wf_Nio_ErrorFunc errorfn);
int Wf_Nio_Read_fd (WF_NIO *asyncio, Wf_Nio_ReadFunc readfn, Wf_Nio_ErrorFunc errorfn);
int Wf_Nio_Accept_fd (WF_NIO *asyncio, Wf_Nio_AcceptFunc acceptfn, Wf_Nio_ErrorFunc errorfn);
int Set_Nio (int fd);
int Wf_Run_Event ();
int Wf_Nio_Listen_Port (unsigned short port, int max_connect);
int Change_Socket_Opt (int fd, int keepalive, int keepidle, int keepintvl, int keepcnt);

#endif
