#ifndef __WF_ASYNC_IO_H__
#define __WF_ASYNC_IO_H__

#include <sys/epoll.h>

typedef struct WFASYNCIO WFASYNCIO;

typedef int (*ReadFunc) (WFASYNCIO *asyncio, int fd, void *ptr);
typedef int (*WriteFunc) (WFASYNCIO *asyncio, int fd, void *ptr);
typedef int (*ErrorFunc) (WFASYNCIO *asyncio, int fd, void *ptr, uint32_t events);

int Init_Wf_Async_Io ();
int Wf_Add_Epoll_Fd (int fd, ReadFunc readfn, WriteFunc writefn, ErrorFunc errorfn, void *ptr);
int Wf_Mod_Epoll_Fd (WFASYNCIO *asyncio, ReadFunc readfn, WriteFunc writefn, ErrorFunc errorfn, void *ptr);
int Wf_Del_Epoll_Fd (WFASYNCIO *asyncio);
int Wf_Run_Event ();

#endif
