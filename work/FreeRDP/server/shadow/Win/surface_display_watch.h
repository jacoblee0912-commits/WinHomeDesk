/* Hidden top-level window: message-only windows do not receive display broadcasts.
 * Owned/pumped/destroyed by the capture thread. Never shown or activated. */
#ifndef WHD_DISPLAY_WATCH_H
#define WHD_DISPLAY_WATCH_H
#include <windows.h>
static LRESULT CALLBACK whd_display_watch_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    if(message==WM_NCCREATE)SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)((CREATESTRUCTW*)lparam)->lpCreateParams);
    if(message==WM_DISPLAYCHANGE || (message==WM_ENDSESSION && wparam)) {
        HANDLE stop=(HANDLE)GetWindowLongPtrW(window,GWLP_USERDATA);
        if(stop)SetEvent(stop);
    }
    return DefWindowProcW(window,message,wparam,lparam);
}
static HWND whd_display_watch_new(HANDLE stop) {
    const WCHAR* name=L"WinHomeDesk.DisplayWatch";
    WNDCLASSEXW cls={0};cls.cbSize=sizeof(cls);cls.hInstance=GetModuleHandleW(NULL);cls.lpfnWndProc=whd_display_watch_proc;cls.lpszClassName=name;
    if(!RegisterClassExW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return NULL;
    return CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,name,L"",WS_POPUP,0,0,0,0,NULL,NULL,cls.hInstance,stop);
}
static void whd_display_watch_pump(HWND window) {
    MSG message;
    while(PeekMessageW(&message,window,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
}
#endif
