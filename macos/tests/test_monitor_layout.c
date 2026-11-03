#include "../../work/FreeRDP/server/shadow/surface_monitors.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    WhdMonitorLayout l;
    WhdMonitor m[]={{0,0,3840,2160,1,1},{-2560,-400,0,1040,0,2}};
    assert(whd_monitor_layout(m,2,1920,&l));
    assert(l.width==3200 && l.height==1280 && l.left==-2560 && l.top==-400);
    assert(l.pixels[0].left==1280 && l.pixels[0].top==200 && l.pixels[0].right==3200);
    assert(l.pixels[1].left==0 && l.pixels[1].right==1280 && l.pixels[1].top==0);
    WhdMonitor vertical[]={{0,0,1920,1080,1,1},{300,-1920,1380,0,0,2}};
    assert(whd_monitor_layout(vertical,2,0,&l));assert(l.width==1920 && l.height==3000 && l.pixels[0].top==1920);
    WhdMonitor wide[]={{0,0,7680,4320,1,1},{7680,0,15360,4320,0,2}};
    assert(whd_monitor_layout(wide,2,0,&l));assert(l.width==8192 && l.height==2304 && l.pixels[1].left==4096);
    assert(whd_monitor_layout(wide,2,1440,&l));assert(l.width==2880 && l.pixels[1].left==1440);
    m[1].primary=1;assert(!whd_monitor_layout(m,2,1920,&l));m[1].primary=0;
    m[1].right=m[1].left;assert(!whd_monitor_layout(m,2,1920,&l));
    assert(!whd_monitor_layout(m,0,1920,&l));assert(!whd_monitor_layout(m,17,1920,&l));
    puts("PASS monitor layout: negative coordinates, mixed sizes, portrait, per-monitor scaling, bounded canvas and invalid geometry");
}
