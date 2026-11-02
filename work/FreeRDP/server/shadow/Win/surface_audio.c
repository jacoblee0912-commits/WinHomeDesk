/* Surface Desk system-sound producer. Apache-2.0.
 * Audio is queued only for authenticated, activated clients that negotiated rdpsnd.
 * No recordings are stored. Keep the queue bounded and discard old sound packets.
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <winpr/collections.h>
#include "surface_audio.h"
#include "surface_audio_format.h"
#include "surface_wasapi.h"
static AUDIO_FORMAT format={.wFormatTag=1,.nChannels=2,.nSamplesPerSec=SURFACE_AUDIO_RATE,
    .nAvgBytesPerSec=SURFACE_AUDIO_BYTES_PER_SECOND,.nBlockAlign=4,.wBitsPerSample=16,.cbSize=0,.data=NULL};
BOOL surface_audio_enabled(void) {const char* v=getenv("SURFACE_DESK_AUDIO");return v && strcmp(v,"1")==0;}
void surface_audio_formats(rdpShadowSubsystem* base) {base->rdpsndFormats=&format;base->nRdpsndFormats=1;}
static void free_packet(UINT32 id,SHADOW_MSG_OUT* packet) {free(packet);}
static void queue_audio(rdpShadowSubsystem* base,const int16_t* data,UINT32 frames,UINT64 capturedAt,UINT64* queued,UINT64* dropped) {
    rdpShadowServer* server=base->server;
    UINT64 now=GetTickCount64();if(capturedAt>now)capturedAt=now;
    ArrayList_Lock(server->clients);
    for(size_t i=0;i<ArrayList_Count(server->clients);i++) {
        rdpShadowClient* c=ArrayList_GetItem(server->clients,i);
        if(!c->activated || !c->mayView || c->inLobby || !InterlockedCompareExchange(&c->surfaceAudioReady,0,0))continue;
        if(MessageQueue_Size(c->MsgQueue)>=8){(*dropped)++;continue;}
        SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES* p=calloc(1,sizeof(*p)+(size_t)frames*4);
        if(!p){(*dropped)++;continue;}
        p->common.Free=free_packet;p->audio_format=&format;p->buf=p+1;
        p->nFrames=frames;p->surfaceCapturedAt=capturedAt;p->wTimestamp=(UINT16)capturedAt;memcpy(p->buf,data,(size_t)frames*4);
        /* Dispatch consumes the allocation both on success and on failure. */
        if(shadow_client_post_msg(c,NULL,SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES_ID,(SHADOW_MSG_OUT*)p,NULL))(*queued)++;
        else (*dropped)++;
    }
    ArrayList_Unlock(server->clients);
}
DWORD WINAPI surface_audio_thread(LPVOID arg) {
    rdpShadowSubsystem* base=arg;SurfaceWasapi* capture=NULL;
    HRESULT init=CoInitializeEx(NULL,COINIT_MULTITHREADED),hr=S_OK;
    if(FAILED(init)){fprintf(stderr,"SurfaceDesk AUDIO state=error hr=0x%08lx\n",(unsigned long)init);return 0;}
    int16_t* pcm=calloc(16384*2,sizeof(int16_t));int16_t chunk[SURFACE_AUDIO_CHUNK_FRAMES*2];UINT32 pending=0;UINT64 chunkTick=0;
    UINT64 queued=0,dropped=0,frames=0,discontinuities=0,estimated=0;unsigned peak=0;
    ULONGLONG report=GetTickCount64(),deviceCheck=report,retry=0;HRESULT lastError=S_OK;
    if(!pcm){CoUninitialize();return 0;}
    HANDLE timer=CreateWaitableTimerExW(NULL,NULL,0x00000002,TIMER_ALL_ACCESS);
    while(TRUE) {
        if(timer) {
            LARGE_INTEGER due;due.QuadPart=-50000;HANDLE events[]={base->server->StopEvent,timer};
            if(SetWaitableTimer(timer,&due,0,NULL,NULL,FALSE)) {
                if(WaitForMultipleObjects(2,events,FALSE,INFINITE)!=WAIT_OBJECT_0+1)break;
            } else if(WaitForSingleObject(base->server->StopEvent,5)!=WAIT_TIMEOUT)break;
        } else if(WaitForSingleObject(base->server->StopEvent,5)!=WAIT_TIMEOUT)break;
        ULONGLONG now=GetTickCount64();
        if(!capture) {
            if(now<retry)continue;
            capture=surface_wasapi_open(&hr);pending=0;
            if(!capture) {
                if(hr!=lastError){fprintf(stderr,"SurfaceDesk AUDIO state=error hr=0x%08lx retry=2s\n",(unsigned long)hr);fflush(stderr);lastError=hr;}
                retry=now+2000;continue;
            }
            lastError=S_OK;deviceCheck=now;
            fprintf(stderr,"SurfaceDesk AUDIO state=capturing backend=wasapi rate=44100 channels=2 bits=16\n");fflush(stderr);
        }
        if(now-deviceCheck>=2000) {
            deviceCheck=now;
            if(surface_wasapi_default_changed(capture)) {
                surface_wasapi_close(capture);capture=NULL;pending=0;retry=0;
                fprintf(stderr,"SurfaceDesk AUDIO state=device-changed\n");fflush(stderr);continue;
            }
        }
        for(unsigned packets=0;packets<32;packets++) {
            UINT32 count=0;BOOL discontinuity=FALSE;
            int rc=surface_wasapi_read(capture,pcm,16384,&count,&discontinuity,&hr);
            if(rc==0)break;
            if(rc<0) {
                fprintf(stderr,"SurfaceDesk AUDIO state=error hr=0x%08lx retry=2s\n",(unsigned long)hr);fflush(stderr);
                surface_wasapi_close(capture);capture=NULL;pending=0;retry=now+2000;lastError=hr;break;
            }
            frames+=count;UINT64 packetTick=surface_wasapi_timestamp(capture);
            if(!surface_wasapi_timestamp_valid(capture))estimated++;
            if(discontinuity){discontinuities++;pending=0;}
            if(GetTickCount64()-packetTick>80){dropped+=(count+SURFACE_AUDIO_CHUNK_FRAMES-1)/SURFACE_AUDIO_CHUNK_FRAMES;pending=0;continue;}
            for(UINT32 i=0;i<count;i++) {
                if(!pending)chunkTick=packetTick+(UINT64)i*1000/SURFACE_AUDIO_RATE;
                for(unsigned channel=0;channel<2;channel++) {
                    int v=pcm[i*2+channel];unsigned magnitude=(unsigned)(v<0?-v:v);
                    if(magnitude>peak)peak=magnitude;
                    chunk[pending*2+channel]=(int16_t)v;
                }
                if(++pending==SURFACE_AUDIO_CHUNK_FRAMES){queue_audio(base,chunk,SURFACE_AUDIO_CHUNK_FRAMES,chunkTick,&queued,&dropped);pending=0;}
            }
        }
        if(now-report>=2000) {
            fprintf(stderr,"SurfaceDesk AUDIO_STATS frames=%llu queued=%llu dropped=%llu peak=%u discontinuities=%llu timestamp_estimates=%llu\n",
                (unsigned long long)frames,(unsigned long long)queued,(unsigned long long)dropped,peak,(unsigned long long)discontinuities,(unsigned long long)estimated);
            fflush(stderr);report=now;frames=queued=dropped=discontinuities=estimated=0;peak=0;
        }
    }
    if(timer)CloseHandle(timer);
    surface_wasapi_close(capture);free(pcm);CoUninitialize();
    fprintf(stderr,"SurfaceDesk AUDIO state=stopped\n");fflush(stderr);return 0;
}
