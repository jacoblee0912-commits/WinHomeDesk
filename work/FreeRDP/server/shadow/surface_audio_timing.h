/* Surface Desk audio diagnostics. Apache-2.0. No audio samples stored here.
 * Some clients confirm twice (receipt and estimated playback); retain max per block.
 * Client-reported hold time is not a physical speaker/ear latency measurement. */
#ifndef SURFACE_AUDIO_TIMING_H
#define SURFACE_AUDIO_TIMING_H
#ifdef _WIN32
#include <windows.h>
#else
#include <winpr/synch.h>
#include <winpr/sysinfo.h>
#include <winpr/interlocked.h>
#endif
#include <stdlib.h>
typedef struct {BOOL valid,confirmed;UINT16 stamp;UINT64 sent;double age;UINT32 hold,ack;} SurfaceAudioBlock;
typedef struct {CRITICAL_SECTION lock;SurfaceAudioBlock slots[256];} SurfaceAudioTiming;
typedef struct {unsigned sent,confirmed,pending;double age,ageMax,hold,holdMax,ack;} SurfaceAudioReport;
static SurfaceAudioTiming* surface_audio_timing_new(void) {
    SurfaceAudioTiming* t=calloc(1,sizeof(*t));if(!t)return NULL;
    InitializeCriticalSection(&t->lock);return t;
}
static void surface_audio_timing_free(SurfaceAudioTiming* t){if(t){DeleteCriticalSection(&t->lock);free(t);}}
static void surface_audio_timing_sent(SurfaceAudioTiming* t,BYTE block,UINT16 stamp,UINT64 now,UINT64 captured) {
    if(!t)return;EnterCriticalSection(&t->lock);
    SurfaceAudioBlock b={0};b.valid=TRUE;b.stamp=stamp;b.sent=now;b.age=now>=captured?(double)(now-captured):0;
    t->slots[block]=b;LeaveCriticalSection(&t->lock);
}
static void surface_audio_timing_confirm(SurfaceAudioTiming* t,BYTE block,UINT16 stamp,UINT64 now) {
    if(!t)return;EnterCriticalSection(&t->lock);SurfaceAudioBlock* b=&t->slots[block];
    UINT32 hold=(UINT16)(stamp-b->stamp);
    /* Reject stale/impossible matches and handle the 16-bit timestamp wrap. */
    if(b->valid && now>=b->sent && now-b->sent<10000 && hold<10000) {
        b->confirmed=TRUE;if(hold>b->hold)b->hold=hold;
        UINT32 ack=(UINT32)(now-b->sent);if(ack>b->ack)b->ack=ack;
    }
    LeaveCriticalSection(&t->lock);
}
static SurfaceAudioReport surface_audio_timing_report(SurfaceAudioTiming* t,UINT64 now) {
    SurfaceAudioReport r={0};if(!t)return r;EnterCriticalSection(&t->lock);
    for(unsigned i=0;i<256;i++) {
        SurfaceAudioBlock* b=&t->slots[i];if(!b->valid||now<b->sent||now-b->sent>2000)continue;
        r.sent++;r.age+=b->age;if(b->age>r.ageMax)r.ageMax=b->age;
        if(b->confirmed){r.confirmed++;r.hold+=b->hold;r.ack+=b->ack;if(b->hold>r.holdMax)r.holdMax=b->hold;}
        else r.pending++;
    }
    LeaveCriticalSection(&t->lock);
    if(r.sent)r.age/=r.sent;if(r.confirmed){r.hold/=r.confirmed;r.ack/=r.confirmed;}return r;
}
#endif
