#pragma once
// POSIX adapter for the existing room worker. Wake events use nonblocking pipes;
// socket events use poll, so link traffic wakes immediately without a timer spin.
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <vector>
#include <cstdint>
#include <new>
using SOCKET=int;
constexpr int INVALID_SOCKET=-1,SOCKET_ERROR=-1,WSAEWOULDBLOCK=EWOULDBLOCK;
using BOOL=int;using DWORD=uint32_t;
constexpr BOOL TRUE=1,FALSE=0;
constexpr DWORD WAIT_FAILED=0xffffffff;
constexpr long FD_READ=1,FD_WRITE=2,FD_CLOSE=4,FD_ACCEPT=8;
struct MacNetworkEvent {int fd=-1,writer=-1;bool pipe=false;};
using HANDLE=MacNetworkEvent*;using WSAEVENT=HANDLE;
constexpr HANDLE WSA_INVALID_EVENT=nullptr;
struct WSADATA {};
inline int MAKEWORD(int,int){return 0;}
inline int WSAStartup(int,WSADATA*){return 0;}
inline void WSACleanup(){}
inline int WSAGetLastError(){return errno==EINPROGRESS?EWOULDBLOCK:errno;}
inline int closesocket(int fd){return close(fd);}
inline int ioctlsocket(int fd,unsigned long request,unsigned long* value){
    int result=ioctl(fd,request,value),yes=1;setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof(yes));fcntl(fd,F_SETFD,FD_CLOEXEC);return result;
}
inline HANDLE CreateEventW(void*,int,int,void*){
    int fds[2];if(pipe(fds))return nullptr;
    for(auto fd:fds){fcntl(fd,F_SETFL,O_NONBLOCK);fcntl(fd,F_SETFD,FD_CLOEXEC);}
    auto* result=new(std::nothrow) MacNetworkEvent{fds[0],fds[1],true};
    if(!result){close(fds[0]);close(fds[1]);}return result;
}
inline void SetEvent(HANDLE event){char byte=1;while(write(event->writer,&byte,1)<0&&errno==EINTR){}}
inline void CloseHandle(HANDLE event){if(!event)return;if(event->pipe){close(event->fd);close(event->writer);}delete event;}
inline HANDLE WSACreateEvent(){return new(std::nothrow) MacNetworkEvent;}
inline int WSAEventSelect(int fd,HANDLE event,long){event->fd=fd;return 0;}
inline void WSACloseEvent(HANDLE event){CloseHandle(event);}
struct WSANETWORKEVENTS {};
inline void WSAEnumNetworkEvents(int,HANDLE,WSANETWORKEVENTS*){}
inline DWORD WaitForMultipleObjects(DWORD count,HANDLE* events,BOOL,DWORD timeout){
    std::vector<pollfd> fds;fds.reserve(count);
    for(DWORD i=0;i<count;++i)fds.push_back({events[i]->fd,POLLIN,0});
    int result;do{result=poll(fds.data(),fds.size(),int(timeout));}while(result<0&&errno==EINTR);
    if(result<0)return WAIT_FAILED;
    for(DWORD i=0;i<count;++i)if(events[i]->pipe&&(fds[i].revents&POLLIN)){char bytes[256];while(read(events[i]->fd,bytes,sizeof(bytes))>0){}}
    return 0;
}
