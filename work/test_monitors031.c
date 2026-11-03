#include <assert.h>
#include "FreeRDP/server/shadow/Win/surface_monitors.h"
int main(void) {
    WhdLayout l={0};l.count=3;l.all=TRUE;
    l.items[0].source=(RECT){0,0,2560,1440};l.items[0].primary=TRUE;
    l.items[1].source=(RECT){2560,0,5120,1440};
    l.items[2].source=(RECT){-1440,-480,0,2080};
    assert(whd_layout_compute(&l,1440));assert(l.width==3688&&l.height==1440);
    assert(l.bounds.left==-1440&&l.bounds.top==-480);
    POINT p;assert(whd_input_point(&l,0,0,&p));assert(p.x==-1440&&p.y==-480);
    assert(!whd_input_point(&l,l.width-1,0,&p)); // blank top-right region
    assert(!whd_input_point(&l,l.width,0,&p));
    for(UINT i=0;i<l.count;i++) {
        WhdMonitor* m=&l.items[i];
        assert(whd_input_point(&l,m->stream.left,m->stream.top,&p));
        assert(p.x==m->source.left&&p.y==m->source.top);
        assert(whd_input_point(&l,m->stream.right-1,m->stream.bottom-1,&p));
        assert(p.x>=m->source.right-3&&p.x<m->source.right);
        assert(p.y>=m->source.bottom-3&&p.y<m->source.bottom);
        assert(m->stream.right<=l.width&&m->stream.bottom<=l.height);
    }
    WhdLayout changed=l;assert(whd_layout_equal(&l,&changed));changed.items[1].source.left++;assert(!whd_layout_equal(&l,&changed));
    changed=l;changed.items[1].rotation++;assert(!whd_layout_equal(&l,&changed));
    l.count=2;l.items[0].source=(RECT){0,0,1920,1200};l.items[1].source=(RECT){0,-1200,1920,0};
    assert(whd_layout_compute(&l,1920));assert(l.width==1920&&l.height==2400);
    assert(whd_input_point(&l,0,0,&p)&&p.y==-1200);
    l.items[1].primary=TRUE;assert(!whd_layout_compute(&l,1920));l.items[1].primary=FALSE;
    l.items[1].source=(RECT){0,0,0,100};assert(!whd_layout_compute(&l,1920));
    l.count=17;assert(!whd_layout_compute(&l,1920));
    l.count=1;l.items[0].source=(RECT){0,0,2560,1440};assert(whd_layout_compute(&l,1920));assert(l.width==1920&&l.height==1080);
    assert(whd_layout_compute(&l,0));assert(l.width==2560&&l.height==1440);
    l.count=2;l.items[1].source=(RECT){2560,0,7680,2880};l.items[1].primary=FALSE;
    assert(whd_layout_compute(&l,0));assert((int64_t)((l.width+15)&~15)*((l.height+15)&~15)<=9437184);
    printf("PASS actual layout and input functions: three monitors, negative X/Y, vertical stacking, gaps, bounds, topology changes, invalid layouts, primary and native sizing\n");
    return 0;
}
