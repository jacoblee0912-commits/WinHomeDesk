#include <stdio.h>
#include "FreeRDP/server/shadow/Win/surface_keyboard.h"
static unsigned observed,down,up;
static LRESULT CALLBACK hook(int n,WPARAM w,LPARAM l) {
    KBDLLHOOKSTRUCT* e=(KBDLLHOOKSTRUCT*)l;
    if(n>=0 && e->dwExtraInfo==0x5748444B) {
        printf("HOOK vk=%lu scan=%lu flags=%lu\n",e->vkCode,e->scanCode,e->flags);
        if(e->vkCode==VK_HANGUL || e->vkCode==VK_HANJA) {
            observed++;if(e->flags&LLKHF_UP)up++;else down++;
        }
        return 1; /* Consume only this test's tagged events; never type in user apps. */
    }
    return CallNextHookEx(NULL,n,w,l);
}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
    BYTE held=0;INPUT e[2];HKL en=(HKL)(ULONG_PTR)0x04090409;
    CHECK(surface_keyboard_events(&held,0,0x72,en,e)==2);
    CHECK(e[0].u.ki.wVk==VK_HANGUL && e[0].u.ki.dwFlags==0 && e[1].u.ki.dwFlags==KEYEVENTF_KEYUP);
    CHECK(surface_keyboard_events(&held,0,0x72,en,e)==0); /* held repeat */
    CHECK(surface_keyboard_events(&held,KBD_FLAGS_RELEASE,0x72,en,e)==0 && held==0);
    CHECK(surface_keyboard_events(&held,KBD_FLAGS_RELEASE,0x72,en,e)==2 && held==0);
    CHECK(surface_keyboard_events(&held,KBD_FLAGS_RELEASE|KBD_FLAGS_EXTENDED,0xF2,en,e)==2);
    CHECK(surface_keyboard_events(&held,KBD_FLAGS_RELEASE,0xF1,en,e)==2 && e[0].u.ki.wVk==VK_HANJA);
    CHECK(surface_keyboard_events(&held,0,0x71,en,e)==2 && e[0].u.ki.wVk==VK_HANJA);
    CHECK(surface_keyboard_events(&held,KBD_FLAGS_RELEASE,0x71,en,e)==0 && held==0);
    CHECK(surface_keyboard_events(&held,KBD_FLAGS_EXTENDED,0x38,en,e)==1 && e[0].u.ki.wScan==0x38 && e[0].u.ki.wVk==0);
    CHECK(e[0].u.ki.dwFlags==(KEYEVENTF_SCANCODE|KEYEVENTF_EXTENDEDKEY));
    CHECK(surface_keyboard_events(&held,0,0x1E,en,e)==1 && e[0].u.ki.wScan==0x1E);
    CHECK(surface_keyboard_events(&held,KBD_FLAGS_RELEASE,0x1E,en,e)==1 && (e[0].u.ki.dwFlags&KEYEVENTF_KEYUP));
    HHOOK h=SetWindowsHookExW(WH_KEYBOARD_LL,hook,GetModuleHandle(NULL),0);CHECK(h!=NULL);
    for(int j=0;j<2;j++) {
        CHECK(surface_keyboard_events(&held,KBD_FLAGS_RELEASE,(BYTE)(0xF1+j),en,e)==2);
        CHECK(SendInput(2,e,sizeof(INPUT))==2);
        DWORD until=GetTickCount()+150;MSG m;
        while((LONG)(until-GetTickCount())>0){while(PeekMessage(&m,NULL,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessage(&m);}Sleep(1);}
    }
    UnhookWindowsHookEx(h);
    printf("observed=%u down=%u up=%u\n",observed,down,up);
    if(observed)CHECK(observed==4 && down==2 && up==2);
    puts("Keyboard: normalized/raw/release-only Korean keys, repeat suppression, balanced down/up, ordinary key and AltGr preservation PASS");
    if(observed)puts("Windows SendInput -> low-level hook: VK_HANGUL/VK_HANJA down+up confirmed (test events consumed)");
    else puts("SKIP: no keyboard hook delivery on this execution desktop; SendInput accepted the events but actual IME switching is UNVERIFIED");
    return 0;
}
