/* WinHomeDesk: bounded, credential-free TCP/X.224 diagnostic. Apache-2.0. */
#ifndef WHD_CONNECTION_CHECK_H
#define WHD_CONNECTION_CHECK_H
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
typedef struct { int connected, rdp, protocol, error; const char *stage; } WHDConnectionCheck;
static inline int64_t whd_now_ms(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
    return (int64_t)ts.tv_sec*1000 + ts.tv_nsec/1000000;
}
static inline int whd_ready(int fd, short events, int64_t deadline) {
    for (;;) {
        int64_t left=deadline-whd_now_ms();
        if(left<=0) { errno=ETIMEDOUT; return -1; }
        struct pollfd p={.fd=fd,.events=events};
        int rc=poll(&p,1,(int)left);
        if(rc<0 && errno==EINTR) continue;
        if(rc==0) errno=ETIMEDOUT;
        return rc>0?0:-1;
    }
}
static inline int whd_transfer(int fd, unsigned char *data, size_t length, int writing, int64_t deadline) {
    size_t offset=0;
    while(offset<length) {
        if(whd_ready(fd,writing?POLLOUT:POLLIN,deadline)<0) return -1;
        ssize_t count=writing?send(fd,data+offset,length-offset,0):recv(fd,data+offset,length-offset,0);
        if(count<0 && (errno==EINTR || errno==EAGAIN || errno==EWOULDBLOCK)) continue;
        if(count<=0) { if(!count) errno=ECONNRESET; return -1; }
        offset+=(size_t)count;
    }
    return 0;
}
static inline WHDConnectionCheck whd_check_endpoint(const char *host,int port) {
    WHDConnectionCheck result={.protocol=-1,.stage="address"};
    if(!host || !*host || port<1 || port>65535) { result.error=EINVAL; return result; }
    char service[8]; snprintf(service,sizeof(service),"%d",port);
    struct addrinfo hint={.ai_family=AF_UNSPEC,.ai_socktype=SOCK_STREAM}, *addresses=NULL;
    int gai=getaddrinfo(host,service,&hint,&addresses);
    if(gai) { result.error=EHOSTUNREACH; result.stage="DNS"; return result; }
    int64_t deadline=whd_now_ms()+4000;
    int fd=-1;
    result.stage="TCP";
    for(struct addrinfo *address=addresses; address && whd_now_ms()<deadline; address=address->ai_next) {
        fd=socket(address->ai_family,address->ai_socktype,address->ai_protocol);
        if(fd<0) { result.error=errno; continue; }
        int one=1; setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof(one));
        if(fcntl(fd,F_SETFL,O_NONBLOCK)<0) { result.error=errno; close(fd); fd=-1; continue; }
        int rc=connect(fd,address->ai_addr,address->ai_addrlen);
        if(rc<0 && errno==EINPROGRESS && whd_ready(fd,POLLOUT,deadline)==0) {
            int error=0; socklen_t length=sizeof(error);
            if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&length)==0) { errno=error; rc=error?-1:0; }
        }
        if(rc==0) break;
        result.error=errno; close(fd); fd=-1;
    }
    freeaddrinfo(addresses);
    if(fd<0) { if(!result.error) result.error=ETIMEDOUT; return result; }
    result.connected=1; result.error=0; result.stage="RDP";
    unsigned char packet[4096]={3,0,0,19,14,0xe0,0,0,0,0,0,1,0,8,0,3,0,0,0};
    if(whd_transfer(fd,packet,19,1,deadline)<0 || whd_transfer(fd,packet,4,0,deadline)<0) result.error=errno;
    else {
        unsigned length=((unsigned)packet[2]<<8)|packet[3];
        if(packet[0]!=3 || packet[1]!=0 || length<11 || length>sizeof(packet)) result.error=EPROTO;
        else if(whd_transfer(fd,packet+4,length-4,0,deadline)<0) result.error=errno;
        else if(packet[5]!=0xd0) result.error=EPROTO;
        else if(length>=19 && packet[length-8]==2 && packet[length-6]==8 && packet[length-5]==0) {
            uint32_t protocol=(uint32_t)packet[length-4] | (uint32_t)packet[length-3]<<8 | (uint32_t)packet[length-2]<<16 | (uint32_t)packet[length-1]<<24;
            if(protocol==0 || protocol==1 || protocol==2 || protocol==8) { result.rdp=1; result.protocol=(int)protocol; }
            else result.error=EPROTO;
        } else result.error=EPROTO;
    }
    close(fd); return result;
}
#endif
