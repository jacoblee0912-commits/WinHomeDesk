/* Test-build-only source: generates quiet PCM; never captures playback (no feedback loop). */
#include <math.h>
static double audio_test_ms(void){LARGE_INTEGER q,f;QueryPerformanceCounter(&q);QueryPerformanceFrequency(&f);return q.QuadPart*1000.0/f.QuadPart;}
struct SurfaceWasapi {double start;UINT64 frames,tick;};
SurfaceWasapi* surface_wasapi_open(HRESULT* error){SurfaceWasapi* s=calloc(1,sizeof(*s));*error=s?S_OK:E_OUTOFMEMORY;if(s)s->start=audio_test_ms();fprintf(stderr,"SurfaceDesk AUDIO_TEST synthetic_source=1 no_loopback=1\n");return s;}
void surface_wasapi_close(SurfaceWasapi* s){free(s);}
int surface_wasapi_read(SurfaceWasapi* s,int16_t* pcm,UINT32 capacity,UINT32* frames,BOOL* discontinuity,HRESULT* error){
    *frames=0;*discontinuity=FALSE;*error=S_OK;
    if(capacity<SURFACE_AUDIO_CHUNK_FRAMES)return -1;
    if(audio_test_ms()-s->start<(s->frames+SURFACE_AUDIO_CHUNK_FRAMES)*1000.0/SURFACE_AUDIO_RATE)return 0;
    for(unsigned i=0;i<SURFACE_AUDIO_CHUNK_FRAMES;i++)pcm[2*i]=pcm[2*i+1]=(int16_t)(350*sin(6.283185307179586*440*(s->frames+i)/(double)SURFACE_AUDIO_RATE));
    s->frames+=SURFACE_AUDIO_CHUNK_FRAMES;*frames=SURFACE_AUDIO_CHUNK_FRAMES;s->tick=GetTickCount64();return 1;
}
UINT64 surface_wasapi_timestamp(SurfaceWasapi* s){return s->tick;}
BOOL surface_wasapi_timestamp_valid(SurfaceWasapi* s){return TRUE;}
BOOL surface_wasapi_default_changed(SurfaceWasapi* s){return FALSE;}
