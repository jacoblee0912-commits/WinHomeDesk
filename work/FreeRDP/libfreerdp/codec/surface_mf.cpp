/* Surface Desk. Apache-2.0. Hardware MFTs only, NV12 input, low-latency H.264.
 * All calls and destruction belong to the encoder worker's COM apartment. */
#include "surface_mf.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <mferror.h>
#include <codecapi.h>
#include <strmif.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

template<class T> static void release(T*& p){if(p){p->Release();p=NULL;}}
struct SurfaceMF {
    IMFTransform* transform;
    IMFMediaEventGenerator* events;
    IMFActivate* activation;
    HANDLE waitTimer;
    UINT32 width,height,fps,bitrate;
    DWORD inputID,outputID;
    bool needInput,haveOutput,mfStarted,comStarted,sent;
    LONGLONG timestamp;
    BYTE* output;size_t capacity;
};
static HRESULT set64(IMFAttributes* a,REFGUID key,UINT32 hi,UINT32 lo){return a->SetUINT64(key,((UINT64)hi<<32)|lo);}
static void codec_value(IMFTransform* t,REFGUID key,ULONG value,bool boolean=false) {
    ICodecAPI* c=NULL;if(FAILED(t->QueryInterface(IID_ICodecAPI,(void**)&c)))return;
    VARIANT v;VariantInit(&v);V_VT(&v)=boolean?VT_BOOL:VT_UI4;
    if(boolean)V_BOOL(&v)=value?VARIANT_TRUE:VARIANT_FALSE;else V_UI4(&v)=value;
    c->SetValue(&key,&v);c->Release();
}
void surface_mf_free(SurfaceMF* s) {
    if(!s)return;
    if(s->transform){s->transform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH,0);s->transform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING,0);}
    release(s->events);release(s->transform);
    if(s->activation){s->activation->ShutdownObject();release(s->activation);}
    if(s->waitTimer)CloseHandle(s->waitTimer);
    free(s->output);if(s->mfStarted)MFShutdown();if(s->comStarted)CoUninitialize();free(s);
}
static HRESULT configure(SurfaceMF* s,IMFActivate* activation) {
    IMFAttributes* attrs=NULL;IMFMediaType *in=NULL,*out=NULL;HRESULT hr;
    hr=activation->ActivateObject(IID_IMFTransform,(void**)&s->transform);if(FAILED(hr))return hr;
    hr=s->transform->GetAttributes(&attrs);if(FAILED(hr))goto done;
    {UINT32 async=0;hr=attrs->GetUINT32(MF_TRANSFORM_ASYNC,&async);
     if(FAILED(hr)||!async){hr=MF_E_INVALIDREQUEST;goto done;}}
    hr=attrs->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK,TRUE);if(FAILED(hr))goto done;
    hr=s->transform->QueryInterface(IID_IMFMediaEventGenerator,(void**)&s->events);if(FAILED(hr))goto done;
    hr=s->transform->GetStreamIDs(1,&s->inputID,1,&s->outputID);
    if(hr==E_NOTIMPL){s->inputID=s->outputID=0;hr=S_OK;}if(FAILED(hr))goto done;
    codec_value(s->transform,CODECAPI_AVLowLatencyMode,TRUE,true);
    codec_value(s->transform,CODECAPI_AVEncMPVDefaultBPictureCount,0);
    codec_value(s->transform,CODECAPI_AVEncCommonRateControlMode,eAVEncCommonRateControlMode_CBR);
    codec_value(s->transform,CODECAPI_AVEncCommonMeanBitRate,s->bitrate);
    codec_value(s->transform,CODECAPI_AVEncMPVGOPSize,s->fps*2);
    hr=MFCreateMediaType(&out);if(FAILED(hr))goto done;
    hr=out->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video);if(FAILED(hr))goto done;
    hr=out->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_H264);if(FAILED(hr))goto done;
    hr=out->SetUINT32(MF_MT_AVG_BITRATE,s->bitrate);if(FAILED(hr))goto done;
    hr=set64(out,MF_MT_FRAME_SIZE,s->width,s->height);if(FAILED(hr))goto done;
    hr=set64(out,MF_MT_FRAME_RATE,s->fps,1);if(FAILED(hr))goto done;
    hr=set64(out,MF_MT_PIXEL_ASPECT_RATIO,1,1);if(FAILED(hr))goto done;
    hr=out->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive);if(FAILED(hr))goto done;
    hr=out->SetUINT32(MF_MT_MPEG2_PROFILE,66);if(FAILED(hr))goto done; // baseline, no B frames
    hr=s->transform->SetOutputType(s->outputID,out,0);if(FAILED(hr))goto done;
    hr=MFCreateMediaType(&in);if(FAILED(hr))goto done;
    hr=in->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video);if(FAILED(hr))goto done;
    hr=in->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_NV12);if(FAILED(hr))goto done;
    hr=set64(in,MF_MT_FRAME_SIZE,s->width,s->height);if(FAILED(hr))goto done;
    hr=set64(in,MF_MT_FRAME_RATE,s->fps,1);if(FAILED(hr))goto done;
    hr=set64(in,MF_MT_PIXEL_ASPECT_RATIO,1,1);if(FAILED(hr))goto done;
    hr=in->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive);if(FAILED(hr))goto done;
    hr=in->SetUINT32(MF_MT_DEFAULT_STRIDE,s->width);if(FAILED(hr))goto done;
    hr=s->transform->SetInputType(s->inputID,in,0);if(FAILED(hr))goto done;
    hr=s->transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING,0);if(FAILED(hr))goto done;
    hr=s->transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM,0);
done:
    release(attrs);release(in);release(out);return hr;
}
SurfaceMF* surface_mf_new(UINT32 width,UINT32 height,UINT32 fps,UINT32 bitrate,HRESULT* error) {
    if(!width||!height||(width&1)||(height&1)||width>8192||height>8192){*error=E_INVALIDARG;return NULL;}
    SurfaceMF* s=(SurfaceMF*)calloc(1,sizeof(*s));if(!s){*error=E_OUTOFMEMORY;return NULL;}
    HRESULT hr=CoInitializeEx(NULL,COINIT_MULTITHREADED);
    if(FAILED(hr)){*error=hr;free(s);return NULL;}s->comStarted=true;
    hr=MFStartup(MF_VERSION,MFSTARTUP_FULL);if(FAILED(hr)){*error=hr;surface_mf_free(s);return NULL;}s->mfStarted=true;
    s->waitTimer=CreateWaitableTimerExW(NULL,NULL,0x00000002,TIMER_ALL_ACCESS);
    s->width=width;s->height=height;s->fps=fps?fps:30;s->bitrate=bitrate?bitrate:10000000;
    IMFActivate** list=NULL;UINT32 count=0;
    MFT_REGISTER_TYPE_INFO input={MFMediaType_Video,MFVideoFormat_NV12},output={MFMediaType_Video,MFVideoFormat_H264};
    hr=MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER,MFT_ENUM_FLAG_HARDWARE|MFT_ENUM_FLAG_SORTANDFILTER,&input,&output,&list,&count);
    if(SUCCEEDED(hr)&&!count)hr=MF_E_TOPO_CODEC_NOT_FOUND;
    for(UINT32 i=0;i<count;i++) {
        hr=configure(s,list[i]);
        if(SUCCEEDED(hr)) {WCHAR name[256]={0};UINT32 length=0;
            if(SUCCEEDED(list[i]->GetString(MFT_FRIENDLY_NAME_Attribute,name,256,&length))) {
                char utf8[1024]={0};WideCharToMultiByte(CP_UTF8,0,name,-1,utf8,sizeof(utf8),NULL,NULL);
                fprintf(stderr,"SurfaceDesk GPU_DEVICE name=%s\n",utf8);fflush(stderr);
            }
            s->activation=list[i];s->activation->AddRef();break;}
        release(s->events);release(s->transform);list[i]->ShutdownObject();
    }
    for(UINT32 i=0;i<count;i++)list[i]->Release();CoTaskMemFree(list);
    if(FAILED(hr)||!s->transform){*error=FAILED(hr)?hr:E_FAIL;surface_mf_free(s);return NULL;}
    *error=S_OK;return s;
}
static HRESULT event_until(SurfaceMF* s,bool output,ULONGLONG deadline) {
    while(!(output?s->haveOutput:s->needInput)) {
        IMFMediaEvent* event=NULL;HRESULT hr=s->events->GetEvent(MF_EVENT_FLAG_NO_WAIT,&event);
        if(hr==MF_E_NO_EVENTS_AVAILABLE){
            if(GetTickCount64()>=deadline)return HRESULT_FROM_WIN32(WAIT_TIMEOUT);
            LARGE_INTEGER due;due.QuadPart=-10000;
            if(s->waitTimer&&SetWaitableTimer(s->waitTimer,&due,0,NULL,NULL,FALSE))WaitForSingleObject(s->waitTimer,10);
            else Sleep(1);
            continue;
        }
        if(FAILED(hr))return hr;
        MediaEventType type=MEUnknown;HRESULT status=S_OK;
        hr=event->GetType(&type);if(SUCCEEDED(hr))hr=event->GetStatus(&status);release(event);
        if(FAILED(hr))return hr;if(FAILED(status))return status;
        if(type==METransformNeedInput)s->needInput=true;
        if(type==METransformHaveOutput)s->haveOutput=true;
        if(type==MEError)return E_FAIL;
    }
    return S_OK;
}
static bool nal_type(const BYTE* data,size_t length,BYTE type) {
    for(size_t i=0;i+3<length;i++) {
        if(data[i]||data[i+1])continue;
        size_t start=data[i+2]==1?i+3:(i+4<length && data[i+2]==0 && data[i+3]==1?i+4:0);
        if(start && (data[start]&31)==type)return true;
    }
    return false;
}
int surface_mf_encode(SurfaceMF* s,const BYTE* const planes[3],const UINT32 strides[3],BYTE** data,UINT32* length,HRESULT* error) {
    IMFMediaBuffer *buffer=NULL,*outputBuffer=NULL;IMFSample *sample=NULL,*supplied=NULL;
    MFT_OUTPUT_DATA_BUFFER out={};MFT_OUTPUT_STREAM_INFO info={};BYTE* memory=NULL;DWORD current=0,status=0;
    unsigned changes=0;BYTE* header=NULL;UINT32 headerSize=0;
    const DWORD size=s->width*s->height*3/2;ULONGLONG deadline=GetTickCount64()+2000;
    HRESULT hr=event_until(s,false,deadline);if(FAILED(hr))goto done;
    hr=MFCreateMemoryBuffer(size,&buffer);if(FAILED(hr))goto done;
    hr=buffer->Lock(&memory,NULL,NULL);if(FAILED(hr))goto done;
    for(UINT32 y=0;y<s->height;y++)memcpy(memory+y*s->width,planes[0]+y*strides[0],s->width);
    for(UINT32 y=0;y<s->height/2;y++)for(UINT32 x=0;x<s->width/2;x++) {
        size_t pos=(size_t)s->width*s->height+y*s->width+x*2;
        memory[pos]=planes[1][y*strides[1]+x];memory[pos+1]=planes[2][y*strides[2]+x];
    }
    buffer->Unlock();memory=NULL;hr=buffer->SetCurrentLength(size);if(FAILED(hr))goto done;
    hr=MFCreateSample(&sample);if(FAILED(hr))goto done;
    hr=sample->AddBuffer(buffer);if(FAILED(hr))goto done;
    hr=sample->SetSampleTime(s->timestamp);if(FAILED(hr))goto done;
    hr=sample->SetSampleDuration(10000000/s->fps);if(FAILED(hr))goto done;
    hr=s->transform->ProcessInput(s->inputID,sample,0);if(FAILED(hr))goto done;
    s->timestamp+=10000000/s->fps;s->needInput=false;
    hr=event_until(s,true,deadline);if(FAILED(hr))goto done;
read_output:
    hr=s->transform->GetOutputStreamInfo(s->outputID,&info);if(FAILED(hr))goto done;
    out.dwStreamID=s->outputID;
    if(!(info.dwFlags&MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)) {
        hr=MFCreateSample(&supplied);if(FAILED(hr))goto done;
        hr=MFCreateMemoryBuffer(info.cbSize?info.cbSize:size,&outputBuffer);if(FAILED(hr))goto done;
        hr=supplied->AddBuffer(outputBuffer);if(FAILED(hr))goto done;out.pSample=supplied;
    }
    hr=s->transform->ProcessOutput(0,1,&out,&status);s->haveOutput=false;
    if(hr==MF_E_TRANSFORM_STREAM_CHANGE && changes++<3) {
        if(out.pSample&&out.pSample!=supplied)out.pSample->Release();
        release(supplied);release(outputBuffer);release(out.pEvents);memset(&out,0,sizeof(out));
        IMFMediaType* changed=NULL;
        hr=s->transform->GetOutputAvailableType(s->outputID,0,&changed);
        if(SUCCEEDED(hr))hr=s->transform->SetOutputType(s->outputID,changed,0);
        release(changed);if(FAILED(hr))goto done;
        hr=event_until(s,true,deadline);if(FAILED(hr))goto done;
        goto read_output;
    }
    if(FAILED(hr))goto done;if(!out.pSample){hr=E_FAIL;goto done;}
    release(outputBuffer);hr=out.pSample->ConvertToContiguousBuffer(&outputBuffer);if(FAILED(hr))goto done;
    hr=outputBuffer->Lock(&memory,NULL,&current);if(FAILED(hr))goto done;
    // MF H.264 elementary output must carry Annex-B start codes. Never send AVCC as RDP H.264.
    if(current<4 || memory[0] || memory[1] || !(memory[2]==1 || (memory[2]==0&&memory[3]==1))) {hr=MF_E_INVALIDMEDIATYPE;goto done;}
    // A fresh RDP decoder needs an IDR and SPS/PPS, including after CPU fallback/reset.
    if(!s->sent&&!nal_type(memory,current,5)){hr=MF_E_INVALIDMEDIATYPE;goto done;}
    if((!s->sent||nal_type(memory,current,5)) && (!nal_type(memory,current,7)||!nal_type(memory,current,8))) {
        IMFMediaType* type=NULL;hr=s->transform->GetOutputCurrentType(s->outputID,&type);
        if(SUCCEEDED(hr))hr=type->GetAllocatedBlob(MF_MT_MPEG_SEQUENCE_HEADER,&header,&headerSize);
        release(type);if(FAILED(hr))goto done;
        if(headerSize>65536 || !nal_type(header,headerSize,7)||!nal_type(header,headerSize,8)){hr=MF_E_INVALIDMEDIATYPE;goto done;}
    }
    if((size_t)current+headerSize>s->capacity){BYTE* next=(BYTE*)realloc(s->output,(size_t)current+headerSize);if(!next){hr=E_OUTOFMEMORY;goto done;}s->output=next;s->capacity=(size_t)current+headerSize;}
    if(headerSize)memcpy(s->output,header,headerSize);
    memcpy(s->output+headerSize,memory,current);*data=s->output;*length=current+headerSize;s->sent=true;
done:
    CoTaskMemFree(header);
    if(memory&&outputBuffer)outputBuffer->Unlock();
    release(buffer);release(sample);release(outputBuffer);
    if(out.pSample&&out.pSample!=supplied)out.pSample->Release();release(supplied);release(out.pEvents);
    *error=hr;return SUCCEEDED(hr)?1:-1;
}
