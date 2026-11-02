/* Synthetic D3D11 rendering test; does not require desktop duplication permission. */
#include "FreeRDP/server/shadow/Win/surface_dxgi.c"
#include <stdio.h>
static int test(int width,int height) {
    SurfaceDxgi* c=calloc(1,sizeof(*c));c->sourceWidth=64;c->sourceHeight=48;c->width=width;c->height=height;
    HRESULT hr=D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        NULL,0,D3D11_SDK_VERSION,&c->device,NULL,&c->context);
    const char* driver="hardware";
    if(FAILED(hr)) {driver="WARP";hr=D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_WARP,NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        NULL,0,D3D11_SDK_VERSION,&c->device,NULL,&c->context);}
    if(FAILED(hr)){printf("D3D11 device failed: %08lx\n",(unsigned long)hr);surface_dxgi_free(c);return 1;}
    D3D11_TEXTURE2D_DESC desc={0};desc.Width=width;desc.Height=height;
    desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    hr=ID3D11Device_CreateTexture2D(c->device,&desc,NULL,&c->staging);
    if(FAILED(hr))return 2;
    if(width!=64 || height!=48){hr=create_scaler(c);if(FAILED(hr)){printf("Scaler failed %08lx\n",(unsigned long)hr);return 3;}}
    UINT32 source[64*48];const UINT32 colors[4]={0xff1122ee,0xff22ee11,0xffee1122,0xff33aadd};
    for(int y=0;y<48;y++)for(int x=0;x<64;x++)source[y*64+x]=colors[(y>=24)*2+(x>=32)];
    desc.Width=64;desc.Height=48;desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=0;
    D3D11_SUBRESOURCE_DATA initial={source,64*4,0};ID3D11Texture2D* frame=NULL;
    hr=ID3D11Device_CreateTexture2D(c->device,&desc,&initial,&frame);if(FAILED(hr))return 4;
    size_t stride=width*4+16;BYTE* pixels=malloc(stride*height);memset(pixels,0xcd,stride*height);
    hr=copy_frame(c,frame,pixels,stride);if(FAILED(hr))return 5;
    for(int y=0;y<height;y++) {
        for(int x=width*4;x<(int)stride;x++)if(pixels[y*stride+x]!=0xcd)return 6;
        for(int x=0;x<width;x++) {
            if(abs(x-width/2)<2 || abs(y-height/2)<2)continue; /* bilinear edge blend */
            UINT32 actual;memcpy(&actual,pixels+y*stride+x*4,4);
            if(actual!=colors[(y>=height/2)*2+(x>=width/2)]){printf("Pixel mismatch %d,%d %08x\n",x,y,actual);return 7;}
        }
    }
    printf("D3D11 %s copy/resize 64x48 -> %dx%d: pixel colors, orientation, stride padding PASS\n",driver,width,height);
    free(pixels);RELEASE(frame);surface_dxgi_free(c);return 0;
}
int main(void){int r=test(64,48);if(r)return r;r=test(32,24);if(r)return r;return test(20,16);}
