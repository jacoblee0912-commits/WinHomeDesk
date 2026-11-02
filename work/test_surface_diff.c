#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "FreeRDP/server/shadow/Win/surface_diff.h"
int main(void) {
    const int w=2880,h=1920;size_t stride=(size_t)w*4;
    uint8_t* a=calloc(h,stride);uint8_t* b=calloc(h,stride);assert(a&&b);
    SurfaceDirtyRect rect;
    assert(!surface_copy_changed(a,stride,b,stride,w,h,&rect));
    for(int y=900;y<924;y++)memset(b+y*stride+1400*4,123,12*4);
    assert(surface_copy_changed(a,stride,b,stride,w,h,&rect));
    assert(rect.left==1400&&rect.top==900&&rect.right==1412&&rect.bottom==924);
    printf("Small update: old full-frame area=%d, new dirty area=%d pixels\n",w*h,(rect.right-rect.left)*(rect.bottom-rect.top));
    assert(!memcmp(a,b,h*stride));
    assert(!surface_copy_changed(a,stride,b,stride,w,h,&rect));
    memset(b,255,h*stride);assert(surface_copy_changed(a,stride,b,stride,w,h,&rect));
    assert(rect.left==0&&rect.top==0&&rect.right==w&&rect.bottom==h);
    assert(!memcmp(a,b,h*stride));free(a);free(b);
    /* Independent scalar oracle: edge pixels, disjoint changes, odd sizes, padded rows. */
    const int rw=197,rh=113;size_t ds=rw*4+16,ss=rw*4+28;
    a=calloc(rh,ds);b=calloc(rh,ss);assert(a&&b);srand(613);
    for(int n=0;n<400;n++) {
        int changes=n%29;
        for(int i=0;i<changes;i++){int x=rand()%rw,y=rand()%rh;b[y*ss+x*4+(i%4)]^=1+(rand()%255);}
        SurfaceDirtyRect expected={rw,rh,0,0};
        for(int y=0;y<rh;y++)for(int x=0;x<rw;x++)if(memcmp(a+y*ds+x*4,b+y*ss+x*4,4)){
            if(x<expected.left)expected.left=x;if(y<expected.top)expected.top=y;
            if(x+1>expected.right)expected.right=x+1;if(y+1>expected.bottom)expected.bottom=y+1;
        }
        int changed=surface_copy_changed(a,ds,b,ss,rw,rh,&rect);
        assert(changed==(expected.right>expected.left));assert(!memcmp(&rect,&expected,sizeof(rect)));
        for(int y=0;y<rh;y++){
            assert(!memcmp(a+y*ds,b+y*ss,rw*4));
            for(size_t p=rw*4;p<ds;p++)assert(a[y*ds+p]==0);
        }
    }
    free(a);free(b);puts("Dirty rectangles: unchanged/full/partial/random/padded-stride tests passed");return 0;
}
