/* Surface Desk system playback capture, Apache-2.0. Never opens a microphone. */
#define COBJMACROS
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "surface_wasapi.h"
#include "surface_audio_format.h"
#ifdef SURFACE_AUDIO_TEST_SOURCE
#include "surface_audio_test.inc"
#else
struct SurfaceWasapi {
    IMMDeviceEnumerator* enumerator;
    IMMDevice* device;
    IAudioClient* client;
    IAudioCaptureClient* capture;
    LPWSTR id;
    BOOL started;
    UINT64 capturedAt;
    BOOL timestampValid;
};
void surface_wasapi_close(SurfaceWasapi* s) {
    if(!s)return;
    if(s->client && s->started)IAudioClient_Stop(s->client);
    if(s->capture)IAudioCaptureClient_Release(s->capture);
    if(s->client)IAudioClient_Release(s->client);
    if(s->device)IMMDevice_Release(s->device);
    if(s->enumerator)IMMDeviceEnumerator_Release(s->enumerator);
    CoTaskMemFree(s->id);free(s);
}
SurfaceWasapi* surface_wasapi_open(HRESULT* error) {
    SurfaceWasapi* s=calloc(1,sizeof(*s));HRESULT hr=E_OUTOFMEMORY;
    if(!s)goto fail;
    hr=CoCreateInstance(&CLSID_MMDeviceEnumerator,NULL,CLSCTX_INPROC_SERVER,&IID_IMMDeviceEnumerator,(void**)&s->enumerator);
    if(FAILED(hr))goto fail;
    hr=IMMDeviceEnumerator_GetDefaultAudioEndpoint(s->enumerator,eRender,eConsole,&s->device);
    if(FAILED(hr))goto fail;
    hr=IMMDevice_GetId(s->device,&s->id);if(FAILED(hr))goto fail;
    hr=IMMDevice_Activate(s->device,&IID_IAudioClient,CLSCTX_INPROC_SERVER,NULL,(void**)&s->client);
    if(FAILED(hr))goto fail;
    WAVEFORMATEX format={0};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;
    format.nSamplesPerSec=SURFACE_AUDIO_RATE;format.wBitsPerSample=16;format.nBlockAlign=4;format.nAvgBytesPerSec=SURFACE_AUDIO_BYTES_PER_SECOND;
    /* Windows audio engine performs sample-rate, channel and float-to-PCM conversion. */
    hr=IAudioClient_Initialize(s->client,AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK|AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
        400000,0,&format,NULL);
    if(FAILED(hr))goto fail;
    hr=IAudioClient_GetService(s->client,&IID_IAudioCaptureClient,(void**)&s->capture);
    if(FAILED(hr))goto fail;
    hr=IAudioClient_Start(s->client);if(FAILED(hr))goto fail;
    s->started=TRUE;
    UINT32 size=0;REFERENCE_TIME latency=0;
    IAudioClient_GetBufferSize(s->client,&size);IAudioClient_GetStreamLatency(s->client,&latency);
    fprintf(stderr,"SurfaceDesk AUDIO_BUFFER requested_ms=40 actual_ms=%.2f stream_latency_ms=%.2f chunk_ms=10\n",size*1000.0/SURFACE_AUDIO_RATE,latency/10000.0);fflush(stderr);
    *error=S_OK;return s;
fail:
    *error=hr;surface_wasapi_close(s);return NULL;
}
int surface_wasapi_read(SurfaceWasapi* s,int16_t* pcm,UINT32 capacityFrames,
                       UINT32* frames,BOOL* discontinuity,HRESULT* error) {
    UINT32 available=0,count=0;BYTE* data=NULL;DWORD flags=0;UINT64 qpc100ns=0;
    *frames=0;*discontinuity=FALSE;*error=S_OK;
    HRESULT hr=IAudioCaptureClient_GetNextPacketSize(s->capture,&available);
    if(FAILED(hr)){*error=hr;return -1;}if(!available)return 0;
    hr=IAudioCaptureClient_GetBuffer(s->capture,&data,&count,&flags,NULL,&qpc100ns);
    if(FAILED(hr)){*error=hr;return -1;}
    s->capturedAt=GetTickCount64();s->timestampValid=FALSE;
    if(!(flags&AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR) && qpc100ns) {
        LARGE_INTEGER counter,frequency;QueryPerformanceCounter(&counter);QueryPerformanceFrequency(&frequency);
        double age=(counter.QuadPart*10000000.0/frequency.QuadPart-qpc100ns)/10000.0;
        if(age>=0 && age<10000 && (UINT64)age<=s->capturedAt){s->capturedAt-=(UINT64)age;s->timestampValid=TRUE;}
    }
    *discontinuity=(flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)!=0;
    if(count>capacityFrames)hr=HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    else {
        if(flags&AUDCLNT_BUFFERFLAGS_SILENT)memset(pcm,0,(size_t)count*4);
        else if(data)memcpy(pcm,data,(size_t)count*4);
        else hr=E_POINTER;
    }
    HRESULT released=IAudioCaptureClient_ReleaseBuffer(s->capture,count);
    if(FAILED(released))hr=released;
    if(FAILED(hr)){*error=hr;return -1;}
    *frames=count;return 1;
}
BOOL surface_wasapi_default_changed(SurfaceWasapi* s) {
    IMMDevice* device=NULL;LPWSTR id=NULL;BOOL changed=TRUE;
    HRESULT hr=IMMDeviceEnumerator_GetDefaultAudioEndpoint(s->enumerator,eRender,eConsole,&device);
    if(SUCCEEDED(hr))hr=IMMDevice_GetId(device,&id);
    if(SUCCEEDED(hr))changed=wcscmp(id,s->id)!=0;
    CoTaskMemFree(id);if(device)IMMDevice_Release(device);return changed;
}

UINT64 surface_wasapi_timestamp(SurfaceWasapi* s){return s->capturedAt;}
BOOL surface_wasapi_timestamp_valid(SurfaceWasapi* s){return s->timestampValid;}

#endif
