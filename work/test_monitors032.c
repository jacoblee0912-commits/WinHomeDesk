#include <assert.h>
#include "FreeRDP/server/shadow/Win/surface_monitors.h"
int main(void) {
    WhdLayout l={0};l.count=1;l.selected=TRUE;l.items[0].source=(RECT){-1440,-480,0,2080};
    assert(whd_layout_compute(&l,1920));assert(l.width==1080 && l.height==1920);
    POINT p;assert(whd_input_point(&l,0,0,&p)&&p.x==-1440&&p.y==-480);
    assert(whd_input_point(&l,540,960,&p)&&p.x==-720&&p.y==800);
    assert(whd_input_point(&l,1079,1919,&p)&&p.x<0&&p.x>=-2&&p.y>=2078);
    assert(!whd_input_point(&l,1080,1919,&p));
    l.selected=FALSE;assert(!whd_layout_compute(&l,1920));
    whd_dpi_aware();_putenv_s("WINHOME_DESK_MONITOR_DEVICE","");
    WhdLayout all;assert(whd_layout_read(&all,TRUE,1920));
    for(UINT i=0;i<all.count;i++) {
        const WCHAR* name=wcsstr(all.items[i].device,L"DISPLAY");char device[CCHDEVICENAME]={0};
        assert(WideCharToMultiByte(CP_UTF8,0,name,-1,device,sizeof(device),NULL,NULL));
        _putenv_s("WINHOME_DESK_MONITOR_DEVICE",device);assert(whd_layout_read(&l,FALSE,1920));
        assert(l.count==1&&l.selected&&l.detected==all.detected&&EqualRect(&l.bounds,&all.items[i].source));
        assert(whd_input_point(&l,0,0,&p)&&p.x==l.bounds.left&&p.y==l.bounds.top);
    }
    _putenv_s("WINHOME_DESK_MONITOR_DEVICE","DISPLAY999999");assert(!whd_layout_read(&l,TRUE,1920));
    _putenv_s("WINHOME_DESK_MONITOR_DEVICE","");assert(whd_layout_read(&l,FALSE,1920)&&l.count==1&&l.items[0].primary);
    printf("PASS separate-monitor coordinates, portrait negative origins, native device selection, missing target fail-closed, primary fallback after clear\n");return 0;
}
