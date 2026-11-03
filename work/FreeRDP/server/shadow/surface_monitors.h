/* WinHomeDesk monitor geometry. Coordinates use exclusive right/bottom edges. */
#ifndef WHD_SURFACE_MONITORS_H
#define WHD_SURFACE_MONITORS_H
#include <stdint.h>
#include <string.h>
#define WHD_MAX_MONITORS 16
typedef struct { int32_t left,top,right,bottom,primary,id; } WhdMonitor;
typedef struct {
    unsigned count;
    int32_t left,top,sourceWidth,sourceHeight,width,height;
    WhdMonitor source[WHD_MAX_MONITORS], pixels[WHD_MAX_MONITORS];
} WhdMonitorLayout;
static inline int whd_monitor_layout(const WhdMonitor* src,unsigned count,int edge,WhdMonitorLayout* out) {
    if(!src || !out || !count || count>WHD_MAX_MONITORS)return 0;
    WhdMonitorLayout l={0};l.count=count;
    int32_t left=src[0].left,top=src[0].top,right=src[0].right,bottom=src[0].bottom;
    int64_t longest=0;unsigned primary=0;
    for(unsigned i=0;i<count;i++) {
        int64_t w=(int64_t)src[i].right-src[i].left,h=(int64_t)src[i].bottom-src[i].top;
        if(w<4 || h<4 || w>65535 || h>65535)return 0;
        if(src[i].left<left)left=src[i].left;if(src[i].top<top)top=src[i].top;
        if(src[i].right>right)right=src[i].right;if(src[i].bottom>bottom)bottom=src[i].bottom;
        if(w>longest)longest=w;if(h>longest)longest=h;primary+=src[i].primary!=0;
        l.source[i]=src[i];
    }
    int64_t w=(int64_t)right-left,h=(int64_t)bottom-top;
    if(primary!=1 || w>65535 || h>65535)return 0;
    l.left=left;l.top=top;l.sourceWidth=(int32_t)w;l.sourceHeight=(int32_t)h;
    int64_t num=edge>=640 && edge<longest?edge:longest,den=longest;
    int64_t extent=w>h?w:h;
    if(extent*num>8192*den){num=8192;den=extent;}
    l.width=(int32_t)(w*num/den)&~3;l.height=(int32_t)(h*num/den)&~3;
    if(l.width<4 || l.height<4)return 0;
    for(unsigned i=0;i<count;i++) {
        WhdMonitor m=src[i];
        m.left=(int32_t)(((int64_t)m.left-left)*l.width/w);
        m.right=(int32_t)(((int64_t)m.right-left)*l.width/w);
        m.top=(int32_t)(((int64_t)m.top-top)*l.height/h);
        m.bottom=(int32_t)(((int64_t)m.bottom-top)*l.height/h);
        if(m.right-m.left<2 || m.bottom-m.top<2)return 0;
        l.pixels[i]=m;
    }
    *out=l;return 1;
}
#endif
