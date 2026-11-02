/* Single outstanding codec job. RDP I/O stays on its original thread. Apache-2.0. */
#ifndef SURFACE_WORKER_H
#define SURFACE_WORKER_H
#include <windows.h>
#include <stdlib.h>
typedef void (*SurfaceWorkFn)(void*);
typedef struct {HANDLE thread,stop,work,done;SurfaceWorkFn function;void* argument;} SurfaceWorker;
static DWORD WINAPI surface_worker_thread(void* arg) {
    SurfaceWorker* w=arg;HANDLE handles[]={w->stop,w->work};
    while(WaitForMultipleObjects(2,handles,FALSE,INFINITE)==WAIT_OBJECT_0+1) {
        w->function(w->argument);SetEvent(w->done);
    }
    return 0;
}
static SurfaceWorker* surface_worker_new(void) {
    SurfaceWorker* w=calloc(1,sizeof(*w));if(!w)return NULL;
    w->stop=CreateEvent(NULL,TRUE,FALSE,NULL);w->work=CreateEvent(NULL,FALSE,FALSE,NULL);w->done=CreateEvent(NULL,TRUE,TRUE,NULL);
    if(w->stop&&w->work&&w->done)w->thread=CreateThread(NULL,0,surface_worker_thread,w,0,NULL);
    if(!w->thread){if(w->stop)CloseHandle(w->stop);if(w->work)CloseHandle(w->work);if(w->done)CloseHandle(w->done);free(w);return NULL;}
    return w;
}
static BOOL surface_worker_start(SurfaceWorker* w,SurfaceWorkFn fn,void* arg) {
    if(WaitForSingleObject(w->done,0)!=WAIT_OBJECT_0)return FALSE;
    w->function=fn;w->argument=arg;ResetEvent(w->done);return SetEvent(w->work);
}
static void surface_worker_free(SurfaceWorker* w) {
    if(!w)return;
    WaitForSingleObject(w->done,INFINITE);SetEvent(w->stop);WaitForSingleObject(w->thread,INFINITE);
    CloseHandle(w->thread);CloseHandle(w->stop);CloseHandle(w->work);CloseHandle(w->done);free(w);
}
#endif
