/* Real AVC420 wrapper: sparse source updates must still publish the complete
 * reconstructed VT picture. CPU keeps its existing sparse region behavior. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/codec/color.h>
#include "../../work/FreeRDP/libfreerdp/codec/h264.h"

int main(void) {
    enum { WIDTH=802, HEIGHT=602, STRIDE=WIDTH*4+32 };
    RECTANGLE_16 rect={0,0,WIDTH,HEIGHT};
    BYTE* pixels=calloc(STRIDE,HEIGHT); assert(pixels);
    for(int gpu=0;gpu<=1;gpu++) {
        setenv("SURFACE_DESK_GPU",gpu?"1":"0",1);
        H264_CONTEXT* encoder=h264_context_new(TRUE); assert(encoder);
        encoder->BitRate=10000000;encoder->FrameRate=20;encoder->QP=20;
        encoder->RateControlMode=H264_RATECONTROL_VBR;
        encoder->UsageType=H264_SCREEN_CONTENT_REAL_TIME;
        encoder->NumberOfThreads=1;
        assert(h264_context_reset(encoder,WIDTH,HEIGHT));
        for(unsigned frame=0;frame<48;frame++) {
            for(unsigned y=0;y<HEIGHT;y++) for(unsigned x=0;x<WIDTH;x++) {
                BYTE* pixel=pixels+y*STRIDE+x*4;
                pixel[0]=32+(x/64)%4*40;pixel[1]=64+(y/64)%3*40;pixel[2]=128;pixel[3]=255;
                if(x>=32+(frame%8)*32 && x<64+(frame%8)*32 && y>=32 && y<64)
                    pixel[0]=pixel[1]=pixel[2]=240;
            }
            BYTE* data=NULL;UINT32 length=0;RDPGFX_H264_METABLOCK meta={0};
            assert(avc420_compress(encoder,pixels,PIXEL_FORMAT_BGRA32,STRIDE,WIDTH,HEIGHT,&rect,&data,&length,&meta)>0);
            assert(data && length && meta.numRegionRects);
            unsigned area=0;
            for(unsigned r=0;r<meta.numRegionRects;r++) {
                RECTANGLE_16* region=&meta.regionRects[r];
                area+=(region->right-region->left)*(region->bottom-region->top);
            }
            if(gpu) {
                assert(meta.numRegionRects==1 && !memcmp(&meta.regionRects[0],&rect,sizeof(rect)));
            } else if(frame>0) assert(area<WIDTH*HEIGHT);
            free_h264_metablock(&meta);
        }
        h264_context_free(encoder);
    }
    free(pixels);
    puts("PASS AVC420 regions: 48 CPU sparse / 48 GPU full picture, including periodic IDR");
}
