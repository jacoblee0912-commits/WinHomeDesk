/* Shared capture sizing and dirty-region primitives. Apache-2.0. */
#ifndef WHD_MAC_VIDEO_H
#define WHD_MAC_VIDEO_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static unsigned whd_video_fps(const char* value) {
    return value && !strcmp(value,"45") ? 45 : value && !strcmp(value,"30") ? 30 : 20;
}
static unsigned whd_video_edge(const char* value) {
    return value && !strcmp(value,"0") ? 0 : value && !strcmp(value,"1440") ? 1440 : 1920;
}
static int whd_video_size(unsigned nativeWidth, unsigned nativeHeight, unsigned edge,
                          unsigned* width, unsigned* height) {
    if (!nativeWidth || !nativeHeight || nativeWidth > 16384 || nativeHeight > 16384) return 0;
    double scale = edge ? fmin(1.0, (double)edge / fmax(nativeWidth, nativeHeight)) : 1.0;
    *width = ((unsigned)(nativeWidth * scale) + 3) & ~3U;
    *height = ((unsigned)(nativeHeight * scale) + 3) & ~3U;
    return 1;
}
/* Ignore the unused alpha byte. Copy only changed 32x32 tiles, retaining a
 * bounding rectangle (right/bottom exclusive) for all shared RDP codecs. */
static int whd_video_copy(uint8_t* dst, size_t ds, const uint8_t* src, size_t ss,
                          unsigned width, unsigned height, unsigned rect[4]) {
    rect[0]=width; rect[1]=height; rect[2]=rect[3]=0;
    for (unsigned y=0; y<height; y+=32) for (unsigned x=0; x<width; x+=32) {
        unsigned right=x+32<width?x+32:width, bottom=y+32<height?y+32:height;
        int changed=0;
        for (unsigned row=y; row<bottom && !changed; row++) {
            const uint8_t* a=src+row*ss+x*4; const uint8_t* b=dst+row*ds+x*4;
            for (unsigned col=x; col<right; col++, a+=4, b+=4)
                if (a[0]!=b[0] || a[1]!=b[1] || a[2]!=b[2]) { changed=1; break; }
        }
        if (!changed) continue;
        if(x<rect[0]) rect[0]=x; if(y<rect[1]) rect[1]=y;
        if(right>rect[2]) rect[2]=right; if(bottom>rect[3]) rect[3]=bottom;
        for(unsigned row=y; row<bottom; row++) memcpy(dst+row*ds+x*4,src+row*ss+x*4,(right-x)*4);
    }
    return rect[2]>rect[0] && rect[3]>rect[1];
}
#endif
