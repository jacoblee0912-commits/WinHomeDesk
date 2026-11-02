/* Measures only a known synthetic tone from the test server. Stores no audio. */
#include <windows.h>
#include <objbase.h>
#include <stdio.h>
#include <stdint.h>
#include "FreeRDP/server/shadow/Win/surface_wasapi.h"
int main(void) {
    HRESULT hr=CoInitializeEx(NULL,COINIT_MULTITHREADED);if(FAILED(hr))return 1;
    SurfaceWasapi* s=surface_wasapi_open(&hr);if(!s)return 2;
    UINT64 frames=0,active=0,cross=0;int previous=0;int16_t pcm[16384*2];
    ULONGLONG until=GetTickCount64()+6000;
    while(GetTickCount64()<until) {
        UINT32 count=0;BOOL gap=FALSE;int rc=surface_wasapi_read(s,pcm,16384,&count,&gap,&hr);
        if(rc<0)return 3;
        for(unsigned i=0;i<count;i++){int v=pcm[i*2];if(v>20||v< -20)active++;if(previous<0&&v>=20)cross++;if(v>=20)previous=1;else if(v<=-20)previous=-1;}
        frames+=count;if(!count)Sleep(1);
    }
    surface_wasapi_close(s);CoUninitialize();
    double hz=frames?cross*44100.0/frames:0;
    printf("PITCH frames=%llu active=%llu positive_crossings=%llu hz=%.3f expected=440\n",(unsigned long long)frames,(unsigned long long)active,(unsigned long long)cross,hz);
    return frames>48000&&active>1000?0:4;
}
