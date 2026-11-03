/* Exercise the real OpenH264 wrapper with a failing hardware adapter. No GPU
 * fault hooks are added to the production binary. Apache-2.0. */
#define surface_vt_new failing_vt_new
#define surface_vt_encode failing_vt_encode
#define surface_vt_free failing_vt_free
#define g_Subsystem_OpenH264 test_Subsystem_OpenH264
#include "../../work/FreeRDP/libfreerdp/codec/h264_openh264.c"
#include <assert.h>
static int fail_encode, attempts, freed;
SurfaceVT* failing_vt_new(UINT32 w,UINT32 h,UINT32 fps,UINT32 b,LONG* error) {
    attempts++; *error=-1234;
    return fail_encode?(SurfaceVT*)calloc(1,1):NULL;
}
int failing_vt_encode(SurfaceVT* s,const BYTE* const planes[3],const UINT32 strides[3],BYTE** data,UINT32* length,LONG* error) {
    *error=-5678; return -1;
}
void failing_vt_free(SurfaceVT* s) { if(s) freed++; free(s); }
int main(void) {
    setenv("SURFACE_DESK_GPU","1",1);
    for(fail_encode=0;fail_encode<=1;fail_encode++) {
        attempts=freed=0;
        H264_CONTEXT encoder={0},decoder={0};
        encoder.Compressor=TRUE;encoder.width=encoder.height=64;
        encoder.FrameRate=20;encoder.BitRate=1000000;encoder.QP=20;
        encoder.RateControlMode=H264_RATECONTROL_VBR;encoder.UsageType=H264_SCREEN_CONTENT_REAL_TIME;
        encoder.NumberOfThreads=1;encoder.log=decoder.log=WLog_Get("whd.fallback-test");
        assert(openh264_init(&encoder) && openh264_init(&decoder));
        BYTE y[64*64],u[32*32],v[32*32];memset(u,128,sizeof(u));memset(v,128,sizeof(v));
        const BYTE* planes[]={y,u,v};const UINT32 strides[]={64,32,32};
        for(int frame=0;frame<2;frame++) {
            memset(y,64+frame*20,sizeof(y)); BYTE* packet=NULL; UINT32 length=0;
            assert(openh264_compress(&encoder,planes,strides,&packet,&length)>0 && length>0);
            assert(openh264_decompress(&decoder,packet,length)>0);
            assert(abs((int)decoder.pYUVData[0][0]-(64+frame*20))<=3);
        }
        assert(attempts==1 && freed==fail_encode);
        openh264_uninit(&encoder);openh264_uninit(&decoder);
    }
    puts("PASS: GPU init/encode failure falls back once to decodable CPU H.264");
}
