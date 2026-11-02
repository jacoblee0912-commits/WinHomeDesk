/* Per-thread high resolution pacing. Does not change system timer resolution. */
#ifndef SURFACE_PACER_H
#define SURFACE_PACER_H
#include <windows.h>
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
static HANDLE surface_pacer_new(BOOL* highResolution) {
    HANDLE timer=CreateWaitableTimerExW(NULL,NULL,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
    *highResolution=timer!=NULL;
    return timer?timer:CreateWaitableTimerW(NULL,FALSE,NULL);
}
/* TRUE means keep capturing; stop/error returns FALSE. */
static BOOL surface_pacer_wait(HANDLE timer,HANDLE stop,double milliseconds) {
    if(WaitForSingleObject(stop,0)!=WAIT_TIMEOUT)return FALSE;
    if(milliseconds<=0)return TRUE;
    LARGE_INTEGER due;due.QuadPart=-(LONGLONG)(milliseconds*10000.0+1.0);
    if(!SetWaitableTimer(timer,&due,0,NULL,NULL,FALSE)){SetEvent(stop);return FALSE;}
    HANDLE events[]={stop,timer};
    DWORD result=WaitForMultipleObjects(2,events,FALSE,INFINITE);
    if(result==WAIT_FAILED)SetEvent(stop);
    return result==WAIT_OBJECT_0+1;
}
#endif
