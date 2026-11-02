/* Surface Desk, Apache-2.0. BGRX frame differencing with independent row strides. */
#ifndef SURFACE_DESK_DIFF_H
#define SURFACE_DESK_DIFF_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef struct { int left, top, right, bottom; } SurfaceDirtyRect;
static int surface_copy_changed(uint8_t* dst, size_t dst_stride, const uint8_t* src,
                                size_t src_stride, int width, int height,
                                SurfaceDirtyRect* rect) {
    *rect=(SurfaceDirtyRect){width,height,0,0};
    for(int y=0;y<height;y++) {
        uint8_t* d=dst+(size_t)y*dst_stride;
        const uint8_t* s=src+(size_t)y*src_stride;
        if(!memcmp(d,s,(size_t)width*4))continue;
        int left=0,right=width;
        while(left<right && !memcmp(d+(size_t)left*4,s+(size_t)left*4,4))left++;
        while(right>left && !memcmp(d+(size_t)(right-1)*4,s+(size_t)(right-1)*4,4))right--;
        memcpy(d+(size_t)left*4,s+(size_t)left*4,(size_t)(right-left)*4);
        if(left<rect->left)rect->left=left;
        if(right>rect->right)rect->right=right;
        if(y<rect->top)rect->top=y;
        rect->bottom=y+1;
    }
    return rect->right>rect->left && rect->bottom>rect->top;
}
#endif
