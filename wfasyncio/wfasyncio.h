#ifndef __WF_ASYNC_IO_H__
#define __WF_ASYNC_IO_H__

#include <sys/epoll.h>

#define MAXDATASIZE   2*1024*1024

typedef struct WF_NIO WF_NIO;

typedef int (*Wf_Nio_ReadFunc) (WF_NIO *asyncio, int fd, void *ptr);
typedef int (*Wf_Nio_WriteFunc) (WF_NIO *asyncio, int fd, void *ptr);
typedef int (*Wf_Nio_ErrorFunc) (WF_NIO *asyncio, int fd, void *ptr, uint32_t events);

int Init_Wf_Nio_Io ();
int Wf_Add_Epoll_Fd (int fd, Wf_Nio_ReadFunc readfn, Wf_Nio_WriteFunc writefn, Wf_Nio_ErrorFunc errorfn, void *ptr);
int Wf_Del_Epoll_Fd (WF_NIO *asyncio);
int Wf_Run_Event ();
int Change_Socket_Opt (int fd, int keepalive, int keepidle, int keepintvl, int keepcnt);
int Wf_Nio_Write_fd (WF_NIO *asyncio, unsigned char *data, unsigned int size);


#endif
