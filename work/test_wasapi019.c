#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <stdlib.h>
#include <mmsystem.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "FreeRDP/server/shadow/Win/surface_wasapi.h"
int main(int argc,char**argv) {
    HRESULT hr=CoInitializeEx(NULL,COINIT_MULTITHREADED);
    if(FAILED(hr))return 2;
    SurfaceWasapi* capture=surface_wasapi_open(&hr);
    if(!capture){printf("WASAPI open failed: 0x%08lx\n",(unsigned long)hr);CoUninitialize();return 3;}
    printf("WASAPI open: fixed PCM 44100 Hz stereo 16-bit PASS\n");fflush(stdout);
    if(argc>1){surface_wasapi_close(capture);CoUninitialize();return 0;}
    WAVEFORMATEX format={WAVE_FORMAT_PCM,2,48000,192000,4,16,0};HWAVEOUT output=NULL;
    if(waveOutOpen(&output,WAVE_MAPPER,&format,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR)return 4;
    int16_t* tone=calloc(48000*3*2,sizeof(int16_t));
    for(unsigned i=0;i<48000*3;i++) {
        tone[2*i]=(int16_t)(1200*sin(6.283185307179586*440*i/48000));
        tone[2*i+1]=(int16_t)(1200*sin(6.283185307179586*660*i/48000));
    }
    WAVEHDR header={0};header.lpData=(LPSTR)tone;header.dwBufferLength=48000*3*4;
    if(waveOutPrepareHeader(output,&header,sizeof(header))!=MMSYSERR_NOERROR || waveOutWrite(output,&header,sizeof(header))!=MMSYSERR_NOERROR)return 5;
    int16_t pcm[16384*2];UINT64 total=0,nonzero=0;unsigned peak=0;
    ULONGLONG deadline=GetTickCount64()+3500;
    while(GetTickCount64()<deadline) {
        UINT32 frames=0;BOOL discontinuity=FALSE;int rc=surface_wasapi_read(capture,pcm,16384,&frames,&discontinuity,&hr);
        if(rc<0){printf("WASAPI read error 0x%08lx\n",(unsigned long)hr);break;}
        total+=frames;
        for(unsigned i=0;i<frames*2;i++){unsigned a=(unsigned)abs(pcm[i]);if(a>peak)peak=a;if(a>10)nonzero++;}
        Sleep(5);
    }
    waveOutReset(output);waveOutUnprepareHeader(output,&header,sizeof(header));waveOutClose(output);free(tone);
    surface_wasapi_close(capture);CoUninitialize();
    printf("WASAPI test frames=%llu nonzero=%llu peak=%u\n",(unsigned long long)total,(unsigned long long)nonzero,peak);
    return total>48000 && nonzero>1000?0:6;
}
