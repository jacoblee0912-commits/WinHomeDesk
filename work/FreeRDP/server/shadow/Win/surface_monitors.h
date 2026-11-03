/* WinHomeDesk Windows capture canvas. Apache-2.0.
 * Physical monitor rectangles are exclusive; the RDP canvas is one inclusive
 * MONITOR_DEF at (0,0), including for H.264/GFX. Physical origins stay local.
 */
#ifndef WHD_SURFACE_MONITORS_H
#define WHD_SURFACE_MONITORS_H
#include <windows.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#define WHD_MAX_MONITORS 16
typedef struct { HMONITOR handle; RECT source, stream; WCHAR device[CCHDEVICENAME]; BOOL primary; DWORD rotation; } WhdMonitor;
typedef struct { WhdMonitor items[WHD_MAX_MONITORS]; UINT count,detected; RECT bounds; int width,height; BOOL overflow,all,selected,geometryOnly; } WhdLayout;
static void whd_dpi_aware(void) {
    typedef BOOL (WINAPI *SetContext)(HANDLE);
    SetContext set=(SetContext)GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDpiAwarenessContext");
    if(!set || !set((HANDLE)(INT_PTR)-4))SetProcessDPIAware();
}
static BOOL whd_all_monitors(void) {
    const char* value=getenv("SURFACE_DESK_ALL_MONITORS");
    if(value && !strcmp(value,"0"))return FALSE;
    if(value && !strcmp(value,"1"))return TRUE;
    WCHAR path[32768];DWORD n=GetModuleFileNameW(NULL,path,32768);
    if(!n || n>=32768)return FALSE;
    WCHAR* slash=wcsrchr(path,L'\\');if(!slash)return FALSE;
    if((size_t)(slash-path)+40>=32768)return FALSE;
    wcscpy(slash+1,L"WinHomeDesk-all-monitors.flag");
    DWORD attr=GetFileAttributesW(path);return attr!=INVALID_FILE_ATTRIBUTES && !(attr&FILE_ATTRIBUTE_DIRECTORY);
}
static BOOL CALLBACK whd_enum_monitor(HMONITOR h,HDC dc,LPRECT rect,LPARAM param) {
    WhdLayout* layout=(WhdLayout*)param;MONITORINFOEXW info={0};info.cbSize=sizeof(info);
    if(!GetMonitorInfoW(h,(MONITORINFO*)&info)){layout->overflow=TRUE;return FALSE;}
    layout->detected++;
    if(!layout->all && !(info.dwFlags&MONITORINFOF_PRIMARY))return TRUE;
    if(layout->count>=WHD_MAX_MONITORS){layout->overflow=TRUE;return FALSE;}
    WhdMonitor* m=&layout->items[layout->count++];m->handle=h;m->source=info.rcMonitor;m->primary=!!(info.dwFlags&MONITORINFOF_PRIMARY);
    memcpy(m->device,info.szDevice,sizeof(m->device));
    DEVMODEW mode={0};mode.dmSize=sizeof(mode);
    if(!layout->geometryOnly && EnumDisplaySettingsExW(info.szDevice,ENUM_CURRENT_SETTINGS,&mode,0))m->rotation=mode.dmDisplayOrientation;
    return TRUE;
}
static int whd_monitor_compare(const void* a,const void* b) {
    const WhdMonitor* x=a;const WhdMonitor* y=b;
    const WCHAR* xn=wcsstr(x->device,L"DISPLAY");const WCHAR* yn=wcsstr(y->device,L"DISPLAY");
    if(xn&&yn){long i=wcstol(xn+7,NULL,10),j=wcstol(yn+7,NULL,10);if(i!=j)return i<j?-1:1;}
    return wcscmp(x->device,y->device);
}
// Pure layout arithmetic also used by the synthetic negative-origin tests.
static BOOL whd_layout_compute(WhdLayout* layout,int edge) {
    if(!layout->count || layout->count>WHD_MAX_MONITORS)return FALSE;
    int64_t longest=0;UINT primary=0;RECT bounds=layout->items[0].source;
    for(UINT i=0;i<layout->count;i++) {
        const WhdMonitor* m=&layout->items[i];int64_t w=(int64_t)m->source.right-m->source.left,h=(int64_t)m->source.bottom-m->source.top;
        if(w<2||h<2||w>32768||h>32768)return FALSE;
        if(w>longest)longest=w;if(h>longest)longest=h;if(m->primary)primary++;
        if(m->source.left<bounds.left)bounds.left=m->source.left;if(m->source.top<bounds.top)bounds.top=m->source.top;
        if(m->source.right>bounds.right)bounds.right=m->source.right;if(m->source.bottom>bounds.bottom)bounds.bottom=m->source.bottom;
    }
    if(primary!=1 && !(layout->selected && layout->count==1 && primary==0))return FALSE;
    int64_t sw=(int64_t)bounds.right-bounds.left,sh=(int64_t)bounds.bottom-bounds.top;
    if(sw<2||sh<2||sw>131072||sh>131072)return FALSE;
    int64_t num=(edge>=640 && edge<longest)?edge:longest;
    int64_t w=0,h=0;
    // OpenH264 2.7.0 validates the padded frame against 9437184 pixels.
    // Bound the whole canvas, not each monitor; preserve every monitor and its
    // relative layout instead of disconnecting on the first encoded frame.
    for(;num>0;num--) {
        w=(sw*num/longest)&~3LL;h=(sh*num/longest)&~3LL;
        if(w<=8192&&h<=8192&&((w+15)&~15LL)*((h+15)&~15LL)<=9437184)break;
    }
    if(w<4||h<4||w>8192||h>8192)return FALSE;
    layout->bounds=bounds;layout->width=(int)w;layout->height=(int)h;
    for(UINT i=0;i<layout->count;i++) {
        WhdMonitor* m=&layout->items[i];
        m->stream.left=(LONG)(((int64_t)m->source.left-bounds.left)*w/sw);
        m->stream.right=(LONG)(((int64_t)m->source.right-bounds.left)*w/sw);
        m->stream.top=(LONG)(((int64_t)m->source.top-bounds.top)*h/sh);
        m->stream.bottom=(LONG)(((int64_t)m->source.bottom-bounds.top)*h/sh);
        if(m->stream.right<=m->stream.left || m->stream.bottom<=m->stream.top)return FALSE;
    }
    return TRUE;
}
static BOOL whd_layout_read_ex(WhdLayout* layout,BOOL all,int edge,BOOL geometryOnly) {
    const char* device=getenv("WINHOME_DESK_MONITOR_DEVICE");
    BOOL selected=device && device[0];
    memset(layout,0,sizeof(*layout));layout->all=all || selected;layout->geometryOnly=geometryOnly;
    if(!EnumDisplayMonitors(NULL,NULL,whd_enum_monitor,(LPARAM)layout)||layout->overflow)return FALSE;
    qsort(layout->items,layout->count,sizeof(WhdMonitor),whd_monitor_compare);
    if(selected) {
        BOOL found=FALSE;
        for(UINT i=0;i<layout->count;i++) {
            const WCHAR* name=wcsstr(layout->items[i].device,L"DISPLAY");
            char ascii[CCHDEVICENAME]={0};
            if(name && WideCharToMultiByte(CP_UTF8,0,name,-1,ascii,sizeof(ascii),NULL,NULL) && !strcmp(ascii,device)) {
                layout->items[0]=layout->items[i];layout->count=1;layout->selected=TRUE;found=TRUE;break;
            }
        }
        if(!found){fprintf(stderr,"SurfaceDesk MONITOR_SELECT error=missing-device\n");return FALSE;}
    }
    return whd_layout_compute(layout,edge);
}
static BOOL whd_layout_read(WhdLayout* layout,BOOL all,int edge) {
    return whd_layout_read_ex(layout,all,edge,FALSE);
}
static BOOL whd_layout_equal(const WhdLayout* a,const WhdLayout* b) {
    if(a->count!=b->count||a->detected!=b->detected)return FALSE;
    for(UINT i=0;i<a->count;i++)if(!EqualRect(&a->items[i].source,&b->items[i].source)||
        wcscmp(a->items[i].device,b->items[i].device)||a->items[i].primary!=b->items[i].primary||a->items[i].rotation!=b->items[i].rotation)return FALSE;
    return TRUE;
}
// Input and capture hot paths use USER32 geometry only, never a display-driver
// mode query. Rotation-only changes are caught by the WM_DISPLAYCHANGE watcher.
static BOOL whd_geometry_unchanged(const WhdLayout* before,int edge) {
    WhdLayout now;
    if(!whd_layout_read_ex(&now,before->all,edge,TRUE)||now.count!=before->count)return FALSE;
    for(UINT i=0;i<now.count;i++)now.items[i].rotation=before->items[i].rotation;
    return whd_layout_equal(before,&now);
}
static BOOL whd_input_point(const WhdLayout* l,UINT x,UINT y,POINT* physical) {
    if(x>=(UINT)l->width||y>=(UINT)l->height)return FALSE;
    // Map against the same rounded per-monitor rectangles used by capture.
    for(UINT i=0;i<l->count;i++) {
        const WhdMonitor* m=&l->items[i];
        if(x>=(UINT)m->stream.left&&x<(UINT)m->stream.right&&y>=(UINT)m->stream.top&&y<(UINT)m->stream.bottom) {
            physical->x=m->source.left+(LONG)((int64_t)(x-m->stream.left)*(m->source.right-m->source.left)/(m->stream.right-m->stream.left));
            physical->y=m->source.top+(LONG)((int64_t)(y-m->stream.top)*(m->source.bottom-m->source.top)/(m->stream.bottom-m->stream.top));return TRUE;
        }
    }
    return FALSE; // Gaps are not a physical monitor. Do not move/click there.
}
static void whd_layout_log(const WhdLayout* l) {
    for(UINT i=0;i<l->count;i++) {
        const WhdMonitor* m=&l->items[i];const WCHAR* device=wcsstr(m->device,L"DISPLAY");
        fprintf(stderr,"SurfaceDesk MONITOR index=%u device=%ls primary=%d source=%ld,%ld,%ld,%ld stream=%ld,%ld,%ld,%ld\n",i+1,device?device:m->device,m->primary,m->source.left,m->source.top,m->source.right,m->source.bottom,m->stream.left,m->stream.top,m->stream.right,m->stream.bottom);
    }
    fprintf(stderr,"SurfaceDesk MONITOR_LIST count=%u width=%d height=%d\n",l->count,l->width,l->height);
    fprintf(stderr,"SurfaceDesk MONITOR_CANVAS protocol_monitors=1 physical_monitors=%u origin=0,0\n",l->count);fflush(stderr);
    fprintf(stderr,"SurfaceDesk MONITOR_SELECT version=1\n");fflush(stderr);
}
#endif
