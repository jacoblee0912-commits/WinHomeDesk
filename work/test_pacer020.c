#include <windows.h>
#include <stdio.h>
#include "FreeRDP/server/shadow/Win/surface_pacer.h"
static double ms(void){LARGE_INTEGER n,f;QueryPerformanceCounter(&n);QueryPerformanceFrequency(&f);return n.QuadPart*1000.0/f.QuadPart;}
static DWORD WINAPI stop_soon(LPVOID h){Sleep(50);SetEvent(h);return 0;}
int main(void) {
 BOOL high=FALSE;HANDLE timer=surface_pacer_new(&high),stop=CreateEvent(NULL,TRUE,FALSE,NULL);
 if(!timer||!stop)return 1;
 int rates[]={20,30,45};
 for(int i=0;i<3;i++) {
  double start=ms(),next=start,interval=1000.0/rates[i];
  for(int n=0;n<30;n++){next+=interval;if(!surface_pacer_wait(timer,stop,next-ms()))return 2;}
  double elapsed=ms()-start;
  printf("PACER target=%d actual=%.2f high_resolution=%d\n",rates[i],30000.0/elapsed,high);
  if(elapsed<30*interval*.95 || elapsed>30*interval*2)return 3;
 }
 HANDLE thread=CreateThread(NULL,0,stop_soon,stop,0,NULL);double start=ms();
 BOOL result=surface_pacer_wait(timer,stop,10000);double elapsed=ms()-start;
 WaitForSingleObject(thread,1000);CloseHandle(thread);CloseHandle(timer);CloseHandle(stop);
 if(result||elapsed>1000)return 4;
 puts("PASS: paced 20/30/45 FPS and stop interrupts a long wait");return 0;
}
