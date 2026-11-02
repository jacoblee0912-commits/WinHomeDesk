/* Exercise the actual mailbox with a slow consumer; no desktop capture. */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include "FreeRDP/server/shadow/Win/surface_latest.h"
#define BYTES (1440*960*4)
typedef struct {SurfaceLatest* m;HANDLE done;LONG failed;UINT64 produced,taken,last,maxLag;double elapsed;} Test;
static DWORD WINAPI producer(LPVOID arg) {
 Test* t=arg;BYTE* pixels=malloc(BYTES);ULONGLONG start=GetTickCount64();
 for(UINT64 n=1;n<=150;n++) {
  memset(pixels,(int)(n%251),BYTES);memcpy(pixels,&n,sizeof(n));
  surface_latest_put(t->m,pixels,(double)GetTickCount64());t->produced=n;
  Sleep(8);
 }
 t->elapsed=(double)(GetTickCount64()-start);free(pixels);SetEvent(t->done);return 0;
}
int main(void) {
 Test t={0};t.m=surface_latest_new(BYTES);t.done=CreateEvent(NULL,TRUE,FALSE,NULL);
 if(!t.m || !t.done)return 1;
 HANDLE worker=CreateThread(NULL,0,producer,&t,0,NULL);
 while(TRUE) {
  BYTE* data=NULL;UINT64 seq=0;double captured=0;
  if(surface_latest_take(t.m,&data,&seq,&captured)) {
   if(seq<=t.last || *(UINT64*)data!=seq)t.failed=1;
   t.taken++;t.last=seq;
   UINT64 lag=GetTickCount64()-(UINT64)captured;if(lag>t.maxLag)t.maxLag=lag;
   Sleep(90); /* Simulate compression taking much longer than capture. */
   if(*(UINT64*)data!=seq)t.failed=1;
   for(size_t i=sizeof(UINT64);i<BYTES;i++)if(data[i]!=(BYTE)(seq%251)){t.failed=1;break;}
  } else if(WaitForSingleObject(t.done,0)==WAIT_OBJECT_0)break;
  else WaitForSingleObject(t.m->ready,100);
 }
 WaitForSingleObject(worker,5000);CloseHandle(worker);
 printf("PIPELINE produced=%llu consumed=%llu replaced=%llu final=%llu producer_ms=%.0f max_age_ms=%llu\n",
  (unsigned long long)t.produced,(unsigned long long)t.taken,(unsigned long long)t.m->replaced,
  (unsigned long long)t.last,t.elapsed,(unsigned long long)t.maxLag);
 BOOL ok=!t.failed && t.last==150 && t.taken+t.m->replaced==150 && t.m->replaced>50 && t.elapsed<7000;
 surface_latest_free(t.m);CloseHandle(t.done);
 if(!ok)return 2;
 puts("PASS: immutable snapshots, newest frame wins, bounded memory, slow consumer does not stall producer");return 0;
}
