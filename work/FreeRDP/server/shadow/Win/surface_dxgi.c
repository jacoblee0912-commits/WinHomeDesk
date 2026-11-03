/* Surface Desk DirectX capture, Apache-2.0.
 * A single capture worker owns the immediate context. GPU bilinear resizing is
 * followed by CPU readback for the existing RDP bitmap encoder. This is not a
 * GPU video encoder. Rotated/unsupported outputs fall back to GDI in the caller.
 */
#define COBJMACROS
#include "surface_dxgi.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <stdlib.h>
#include <string.h>

struct SurfaceDxgi {
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    IDXGIOutputDuplication* duplication;
    ID3D11Texture2D *source, *scaled, *staging;
    ID3D11ShaderResourceView* srv;
    ID3D11RenderTargetView* rtv;
    ID3D11VertexShader* vs;
    ID3D11PixelShader* ps;
    ID3D11SamplerState* sampler;
    int sourceWidth, sourceHeight, width, height;
    BOOL haveFrame;
};
#define RELEASE(p) do { if(p) { (p)->lpVtbl->Release(p); (p)=NULL; } } while(0)

void surface_dxgi_free(SurfaceDxgi* c) {
    if(!c)return;
    if(c->context)ID3D11DeviceContext_ClearState(c->context);
    RELEASE(c->sampler); RELEASE(c->ps); RELEASE(c->vs); RELEASE(c->rtv); RELEASE(c->srv);
    RELEASE(c->staging); RELEASE(c->scaled); RELEASE(c->source);
    RELEASE(c->duplication); RELEASE(c->context); RELEASE(c->device); free(c);
}

static HRESULT create_scaler(SurfaceDxgi* c) {
    static const char shader[]=
        "Texture2D image:register(t0); SamplerState sampling:register(s0);"
        "struct V {float4 pos:SV_POSITION; float2 uv:TEXCOORD0;};"
        "V vertex(uint id:SV_VertexID) {V v; float2 uv=float2((id<<1)&2,id&2);"
        "v.pos=float4(uv*float2(2,-2)+float2(-1,1),0,1); v.uv=uv; return v;}"
        "float4 pixel(V v):SV_TARGET {return float4(image.Sample(sampling,v.uv).rgb,1);}";
    ID3DBlob *code=NULL, *errors=NULL;
    HRESULT hr=D3DCompile(shader,sizeof(shader)-1,NULL,NULL,NULL,"vertex","vs_4_0",
                         D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
    RELEASE(errors);
    if(FAILED(hr))return hr;
    hr=ID3D11Device_CreateVertexShader(c->device,ID3D10Blob_GetBufferPointer(code),
                                     ID3D10Blob_GetBufferSize(code),NULL,&c->vs);
    RELEASE(code); if(FAILED(hr))return hr;
    hr=D3DCompile(shader,sizeof(shader)-1,NULL,NULL,NULL,"pixel","ps_4_0",
                  D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
    RELEASE(errors);
    if(FAILED(hr))return hr;
    hr=ID3D11Device_CreatePixelShader(c->device,ID3D10Blob_GetBufferPointer(code),
                                    ID3D10Blob_GetBufferSize(code),NULL,&c->ps);
    RELEASE(code); if(FAILED(hr))return hr;
    D3D11_SAMPLER_DESC sampler={0};
    sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.ComparisonFunc=D3D11_COMPARISON_NEVER;sampler.MaxLOD=D3D11_FLOAT32_MAX;
    hr=ID3D11Device_CreateSamplerState(c->device,&sampler,&c->sampler);
    if(FAILED(hr))return hr;
    D3D11_TEXTURE2D_DESC texture={0};
    texture.Width=c->sourceWidth;texture.Height=c->sourceHeight;
    texture.MipLevels=texture.ArraySize=texture.SampleDesc.Count=1;
    texture.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    texture.Usage=D3D11_USAGE_DEFAULT;texture.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    hr=ID3D11Device_CreateTexture2D(c->device,&texture,NULL,&c->source);
    if(FAILED(hr))return hr;
    hr=ID3D11Device_CreateShaderResourceView(c->device,(ID3D11Resource*)c->source,NULL,&c->srv);
    if(FAILED(hr))return hr;
    texture.Width=c->width;texture.Height=c->height;texture.BindFlags=D3D11_BIND_RENDER_TARGET;
    hr=ID3D11Device_CreateTexture2D(c->device,&texture,NULL,&c->scaled);
    if(FAILED(hr))return hr;
    return ID3D11Device_CreateRenderTargetView(c->device,(ID3D11Resource*)c->scaled,NULL,&c->rtv);
}

SurfaceDxgi* surface_dxgi_create(int sourceWidth,int sourceHeight,int width,int height,HRESULT* error) {
    return surface_dxgi_create_output(MonitorFromPoint((POINT){0,0},MONITOR_DEFAULTTOPRIMARY),sourceWidth,sourceHeight,width,height,error);
}
SurfaceDxgi* surface_dxgi_create_output(HMONITOR monitor,int sourceWidth,int sourceHeight,int width,int height,HRESULT* error) {
    IDXGIFactory1* factory=NULL;
    IDXGIAdapter1* adapter=NULL;
    IDXGIOutput* output=NULL;
    IDXGIOutput1* output1=NULL;
    SurfaceDxgi* c=calloc(1,sizeof(*c));
    HRESULT hr=E_OUTOFMEMORY;
    if(!c)goto out;
    c->sourceWidth=sourceWidth;c->sourceHeight=sourceHeight;c->width=width;c->height=height;
    hr=CreateDXGIFactory1(&IID_IDXGIFactory1,(void**)&factory);
    if(FAILED(hr))goto out;
    BOOL found=FALSE;
    for(UINT ai=0; !found;ai++) {
        hr=IDXGIFactory1_EnumAdapters1(factory,ai,&adapter);
        if(FAILED(hr))break;
        for(UINT oi=0;;oi++) {
            hr=IDXGIAdapter1_EnumOutputs(adapter,oi,&output);
            if(FAILED(hr))break;
            DXGI_OUTPUT_DESC desc={0};
            hr=IDXGIOutput_GetDesc(output,&desc);
            if(SUCCEEDED(hr) && desc.AttachedToDesktop && desc.Monitor==monitor) {
                found=TRUE;
                if(desc.Rotation!=DXGI_MODE_ROTATION_IDENTITY && desc.Rotation!=DXGI_MODE_ROTATION_UNSPECIFIED) {
                    hr=DXGI_ERROR_UNSUPPORTED;goto out;
                }
                break;
            }
            RELEASE(output);
        }
        if(!found)RELEASE(adapter);
    }
    if(!found){hr=DXGI_ERROR_NOT_FOUND;goto out;}
    const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1,D3D_FEATURE_LEVEL_10_0};
    hr=D3D11CreateDevice((IDXGIAdapter*)adapter,D3D_DRIVER_TYPE_UNKNOWN,NULL,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,3,D3D11_SDK_VERSION,&c->device,NULL,&c->context);
    if(FAILED(hr))goto out;
    hr=IDXGIOutput_QueryInterface(output,&IID_IDXGIOutput1,(void**)&output1);
    if(FAILED(hr))goto out;
    hr=IDXGIOutput1_DuplicateOutput(output1,(IUnknown*)c->device,&c->duplication);
    if(FAILED(hr))goto out;
    DXGI_OUTDUPL_DESC desc={0};IDXGIOutputDuplication_GetDesc(c->duplication,&desc);
    if(desc.ModeDesc.Width!=(UINT)sourceWidth || desc.ModeDesc.Height!=(UINT)sourceHeight ||
       desc.ModeDesc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM) {hr=DXGI_ERROR_UNSUPPORTED;goto out;}
    D3D11_TEXTURE2D_DESC texture={0};texture.Width=width;texture.Height=height;
    texture.MipLevels=texture.ArraySize=texture.SampleDesc.Count=1;
    texture.Format=DXGI_FORMAT_B8G8R8A8_UNORM;texture.Usage=D3D11_USAGE_STAGING;
    texture.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    hr=ID3D11Device_CreateTexture2D(c->device,&texture,NULL,&c->staging);
    if(FAILED(hr))goto out;
    if(width!=sourceWidth || height!=sourceHeight)hr=create_scaler(c);
out:
    RELEASE(output1); RELEASE(output); RELEASE(adapter); RELEASE(factory);
    *error=hr;
    if(FAILED(hr)){surface_dxgi_free(c);return NULL;}
    return c;
}

static HRESULT copy_frame(SurfaceDxgi* c,ID3D11Texture2D* frame,BYTE* pixels,size_t stride) {
    D3D11_TEXTURE2D_DESC desc={0};ID3D11Texture2D_GetDesc(frame,&desc);
    if(desc.Width!=(UINT)c->sourceWidth || desc.Height!=(UINT)c->sourceHeight ||
        desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM) {return DXGI_ERROR_UNSUPPORTED;}
    if(c->scaled) {
        ID3D11DeviceContext_CopyResource(c->context,(ID3D11Resource*)c->source,(ID3D11Resource*)frame);
        D3D11_VIEWPORT viewport={0,0,(FLOAT)c->width,(FLOAT)c->height,0,1};
        ID3D11DeviceContext_RSSetViewports(c->context,1,&viewport);
        ID3D11DeviceContext_IASetPrimitiveTopology(c->context,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11DeviceContext_VSSetShader(c->context,c->vs,NULL,0);
        ID3D11DeviceContext_PSSetShader(c->context,c->ps,NULL,0);
        ID3D11DeviceContext_PSSetShaderResources(c->context,0,1,&c->srv);
        ID3D11DeviceContext_PSSetSamplers(c->context,0,1,&c->sampler);
        ID3D11DeviceContext_OMSetRenderTargets(c->context,1,&c->rtv,NULL);
        ID3D11DeviceContext_Draw(c->context,3,0);
        ID3D11ShaderResourceView* empty=NULL;
        ID3D11DeviceContext_PSSetShaderResources(c->context,0,1,&empty);
        ID3D11DeviceContext_OMSetRenderTargets(c->context,0,NULL,NULL);
        ID3D11DeviceContext_CopyResource(c->context,(ID3D11Resource*)c->staging,(ID3D11Resource*)c->scaled);
    } else ID3D11DeviceContext_CopyResource(c->context,(ID3D11Resource*)c->staging,(ID3D11Resource*)frame);
    D3D11_MAPPED_SUBRESOURCE map={0};
    HRESULT hr=ID3D11DeviceContext_Map(c->context,(ID3D11Resource*)c->staging,0,D3D11_MAP_READ,0,&map);
    if(FAILED(hr))return hr;
    for(int y=0;y<c->height;y++)memcpy(pixels+(size_t)y*stride,(BYTE*)map.pData+(size_t)y*map.RowPitch,(size_t)c->width*4);
    ID3D11DeviceContext_Unmap(c->context,(ID3D11Resource*)c->staging,0);
    return S_OK;
}

int surface_dxgi_capture(SurfaceDxgi* c,BYTE* pixels,size_t stride,HRESULT* error) {
    IDXGIResource* resource=NULL;ID3D11Texture2D* frame=NULL;
    DXGI_OUTDUPL_FRAME_INFO info={0};
    HRESULT hr=IDXGIOutputDuplication_AcquireNextFrame(c->duplication,0,&info,&resource);
    *error=hr;
    if(hr==DXGI_ERROR_WAIT_TIMEOUT)return 0;
    if(FAILED(hr))return -1;
    int result=-1;
    if(c->haveFrame && info.LastPresentTime.QuadPart==0) {result=0;goto out;}
    hr=IDXGIResource_QueryInterface(resource,&IID_ID3D11Texture2D,(void**)&frame);
    if(FAILED(hr))goto out;
    hr=copy_frame(c,frame,pixels,stride);
    if(FAILED(hr))goto out;
    c->haveFrame=TRUE;result=1;
out:
    RELEASE(frame); RELEASE(resource);
    HRESULT releaseResult=IDXGIOutputDuplication_ReleaseFrame(c->duplication);
    if(FAILED(releaseResult)){hr=releaseResult;result=-1;}
    *error=hr;return result;
}
