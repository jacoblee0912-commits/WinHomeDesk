/* Synthetic moving-image codec roundtrip; no desktop or remote connection. */
#include <freerdp/config.h>
#include <freerdp/codec/h264.h>
#include "FreeRDP/libfreerdp/codec/test/TestFreeRDPCodecH264.c"
static int moving_roundtrip(int mode) {
    const UINT32 width=320,height=180,stride=width*4+32;
    BYTE *src=calloc(stride,height),*dst=calloc(stride,height);
    H264_CONTEXT *enc=h264_context_new(TRUE),*dec=h264_context_new(FALSE);
    if(!src||!dst||!enc||!dec)return 1;
    if(!h264_context_reset(enc,width,height)||!h264_context_reset(dec,width,height)||
       !h264_context_set_option(enc,H264_CONTEXT_OPTION_RATECONTROL,H264_RATECONTROL_VBR)||
       !h264_context_set_option(enc,H264_CONTEXT_OPTION_BITRATE,10000000)||
       !h264_context_set_option(enc,H264_CONTEXT_OPTION_FRAMERATE,30))return 2;
    const RECTANGLE_16 full={0,0,width,height};
    double worst=0;unsigned received=0;
    for(unsigned frame=0;frame<18;frame++) {
        for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++) {
            BYTE* p=src+y*stride+x*4;
            p[0]=(BYTE)(30+(x/32+frame)%4*45);
            p[1]=(BYTE)(40+(y/30+frame)%3*65);
            p[2]=(BYTE)(45+frame%4*45);p[3]=255;
        }
        BYTE *data=NULL,*aux=NULL,op=0;UINT32 length=0,auxlength=0;
        RDPGFX_H264_METABLOCK meta={0},auxmeta={0};
        int rc=mode==0?avc420_compress(enc,src,PIXEL_FORMAT_BGRX32,stride,width,height,&full,&data,&length,&meta):
          avc444_compress(enc,src,PIXEL_FORMAT_BGRX32,stride,width,height,(BYTE)mode,&full,&op,&data,&length,&aux,&auxlength,&meta,&auxmeta);
        if(rc<0)return 3;
        if(rc>0) {
            int result=mode==0?avc420_decompress(dec,data,length,dst,PIXEL_FORMAT_BGRX32,stride,width,height,meta.regionRects,meta.numRegionRects):
             avc444_decompress(dec,op,meta.regionRects,meta.numRegionRects,data,length,auxmeta.regionRects,auxmeta.numRegionRects,aux,auxlength,dst,PIXEL_FORMAT_BGRX32,stride,width,height,mode==2?RDPGFX_CODECID_AVC444v2:RDPGFX_CODECID_AVC444);
            if(result<0)return 4;
            double error=0;
            for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++)for(unsigned c=0;c<3;c++)
                error+=abs((int)src[y*stride+x*4+c]-(int)dst[y*stride+x*4+c]);
            error/=(width*height*3.0);if(error>worst)worst=error;
            if(error>15.0){fprintf(stderr,"mode=%d frame=%u error=%.3f\n",mode,frame,error);return 5;}
            received++;
        }
        free_h264_metablock(&meta);free_h264_metablock(&auxmeta);
    }
    h264_context_free(enc);h264_context_free(dec);free(src);free(dst);
    if(received<10)return 6;
    printf("Moving codec mode=%d decoded=%u/18 maximum mean RGB error=%.3f PASS\n",mode,received,worst);
    return 0;
}
int main(void) {
    if(!testDecodeAvc444(RDPGFX_CODECID_AVC444)||!testDecodeAvc444(RDPGFX_CODECID_AVC444v2)||
       !testEncodeOffsetRegion()||!testEncode(PIXEL_FORMAT_BGRX32,124,54))return 10;
    for(int mode=0;mode<3;mode++){int rc=moving_roundtrip(mode);if(rc)return rc;}
    puts("H264: AVC420/444/v2, moving colors, non-aligned dimensions, padded stride and partial region tests PASS");
    return 0;
}
