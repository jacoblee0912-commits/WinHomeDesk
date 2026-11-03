#include <assert.h>
#include <stdio.h>
#include "../../work/FreeRDP/server/shadow/Mac/mac_video.h"
int main(void) {
    unsigned w,h,r[4];
    assert(whd_video_size(3840,2160,1920,&w,&h) && w==1920 && h==1080);
    assert(whd_video_size(3840,2160,1440,&w,&h) && w==1440 && h==812);
    assert(whd_video_size(3840,2160,0,&w,&h) && w==3840 && h==2160);
    assert(whd_video_size(2160,3840,1920,&w,&h) && w==1080 && h==1920);
    assert(!whd_video_size(0,2160,1920,&w,&h));
    assert(!whd_video_size(999999,2160,0,&w,&h));
    assert(whd_video_fps("45")==45 && whd_video_fps("30")==30 && whd_video_fps("bad")==20);
    assert(whd_video_edge("0")==0 && whd_video_edge("1440")==1440 && whd_video_edge("-1")==1920);
    /* Odd dimensions and distinct padded strides catch edge-tile overruns. */
    unsigned width=67,height=35; size_t ss=288,ds=280;
    uint8_t *src=calloc(height,ss),*dst=calloc(height,ds);
    assert(src && dst);
    assert(!whd_video_copy(dst,ds,src,ss,width,height,r));
    src[4*66+34*ss+2]=123;
    assert(whd_video_copy(dst,ds,src,ss,width,height,r));
    assert(r[0]==64 && r[1]==32 && r[2]==67 && r[3]==35);
    assert(dst[4*66+34*ds+2]==123);
    assert(!whd_video_copy(dst,ds,src,ss,width,height,r));
    src[3]=255; assert(!whd_video_copy(dst,ds,src,ss,width,height,r));
    src[0]=1;src[4*33]=2;
    assert(whd_video_copy(dst,ds,src,ss,width,height,r));
    assert(r[0]==0 && r[1]==0 && r[2]==64 && r[3]==32);
    free(src);free(dst);
    puts("PASS: quality/FPS sizing, portrait/native, dirty tiles, idle, alpha and padded strides");
}
