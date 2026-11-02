/* Real codec worker scheduling with deliberately slow work; no keyboard/mouse injection. */
#include "FreeRDP/server/shadow/surface_worker.h"
#include <stdio.h>
typedef struct {DWORD owner;LONG completed;} Job;
static void slow(void* arg){Job* j=arg;j->owner=GetCurrentThreadId();Sleep(450);InterlockedIncrement(&j->completed);}
static void cleanup(void* arg){Job* j=arg;if(j->owner!=GetCurrentThreadId())abort();InterlockedIncrement(&j->completed);}
int main(void){
    SurfaceWorker* w=surface_worker_new();Job j={0};if(!w)return 1;
    if(!surface_worker_start(w,slow,&j)||surface_worker_start(w,slow,&j))return 2;
    unsigned ticks=0;ULONGLONG start=GetTickCount64();
    while(WaitForSingleObject(w->done,10)==WAIT_TIMEOUT)ticks++;
    if(ticks<10||j.completed!=1||j.owner==GetCurrentThreadId())return 3;
    if(!surface_worker_start(w,cleanup,&j))return 4;
    WaitForSingleObject(w->done,INFINITE);
    if(j.completed!=2)return 5;
    if(!surface_worker_start(w,slow,&j))return 6;
    surface_worker_free(w);if(j.completed!=3)return 7;
    printf("Worker PASS: responsive owner ticks=%u slow_job_ms=450 same-thread cleanup, reject double-submit, join before free, elapsed=%llu\n",ticks,(unsigned long long)(GetTickCount64()-start));return 0;
}
