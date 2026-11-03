#include <assert.h>
#include <windows.h>
static unsigned modeQueries=0;
static BOOL counted_display_settings(LPCWSTR device,DWORD mode,DEVMODEW* output,DWORD flags) {
    modeQueries++;return EnumDisplaySettingsExW(device,mode,output,flags);
}
#define EnumDisplaySettingsExW counted_display_settings
#include "FreeRDP/server/shadow/Win/surface_monitors.h"
#undef EnumDisplaySettingsExW
#include "FreeRDP/server/shadow/Win/surface_display_watch.h"
int main(void) {
    whd_dpi_aware();_putenv_s("WINHOME_DESK_MONITOR_DEVICE","");
    WhdLayout layout;assert(whd_layout_read(&layout,TRUE,1920));assert(modeQueries>0);
    unsigned initial=modeQueries;
    for(int i=0;i<200;i++)assert(whd_geometry_unchanged(&layout,1920));
    assert(modeQueries==initial);
    WhdLayout changed=layout;changed.items[0].source.left++;
    assert(!whd_geometry_unchanged(&changed,1920));assert(modeQueries==initial);
    HANDLE stop=CreateEventW(NULL,TRUE,FALSE,NULL);assert(stop);
    HWND foreground=GetForegroundWindow();HWND watcher=whd_display_watch_new(stop);assert(watcher);
    assert(!IsWindowVisible(watcher));assert(GetForegroundWindow()==foreground);
    assert(WaitForSingleObject(stop,0)==WAIT_TIMEOUT);
    assert(PostMessageW(watcher,WM_DISPLAYCHANGE,32,MAKELPARAM(1920,1080)));
    whd_display_watch_pump(watcher);assert(WaitForSingleObject(stop,0)==WAIT_OBJECT_0);
    DestroyWindow(watcher);CloseHandle(stop);
    printf("PASS 200 topology checks with no driver-mode queries; changed geometry rejected; hidden non-activating watcher stops on display event without changing real displays\n");
    return 0;
}
