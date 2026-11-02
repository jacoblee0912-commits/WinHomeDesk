#include <stdio.h>
#include "FreeRDP/server/shadow/Win/surface_keyboard.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
    BYTE held=0;INPUT e[2];HKL en=(HKL)(ULONG_PTR)0x04090409,kr=(HKL)(ULONG_PTR)0x04120412;
    CHECK(surface_keyboard_events_ex(&held,KBD_FLAGS_EXTENDED,0x38,en,TRUE,e)==2);
    CHECK(e[0].u.ki.wVk==VK_HANGUL && !(e[0].u.ki.dwFlags & KEYEVENTF_SCANCODE));
    CHECK(e[1].u.ki.wVk==VK_HANGUL && e[1].u.ki.dwFlags==KEYEVENTF_KEYUP);
    CHECK(surface_keyboard_events_ex(&held,KBD_FLAGS_EXTENDED,0x38,en,TRUE,e)==0);
    CHECK(surface_keyboard_events_ex(&held,KBD_FLAGS_EXTENDED|KBD_FLAGS_RELEASE,0x38,kr,TRUE,e)==0 && held==0);
    CHECK(surface_keyboard_events_ex(&held,KBD_FLAGS_EXTENDED,0x38,kr,TRUE,e)==2);
    CHECK(surface_keyboard_events_ex(&held,KBD_FLAGS_EXTENDED|KBD_FLAGS_RELEASE,0x38,en,TRUE,e)==0 && held==0);
    CHECK(surface_keyboard_events_ex(&held,KBD_FLAGS_EXTENDED,0x38,en,FALSE,e)==1);
    CHECK(e[0].u.ki.wVk==0 && e[0].u.ki.wScan==0x38 && e[0].u.ki.dwFlags==(KEYEVENTF_SCANCODE|KEYEVENTF_EXTENDEDKEY));
    CHECK(surface_keyboard_events_ex(&held,KBD_FLAGS_EXTENDED,0x38,kr,FALSE,e)==1 && e[0].u.ki.wVk==0);
    CHECK(surface_keyboard_events_ex(&held,0,0x38,en,TRUE,e)==1 && e[0].u.ki.wScan==0x38); /* left Alt */
    CHECK(surface_keyboard_events_ex(&held,KBD_FLAGS_RELEASE,0x38,en,TRUE,e)==1 && (e[0].u.ki.dwFlags&KEYEVENTF_KEYUP));
    CHECK(surface_keyboard_events_ex(&held,KBD_FLAGS_EXTENDED,0x1D,en,TRUE,e)==1); /* right Ctrl */
    CHECK(surface_keyboard_events_ex(&held,0,0x1E,en,TRUE,e)==1); /* letter */
    CHECK(surface_keyboard_events_ex(&held,KBD_FLAGS_RELEASE,0xF2,en,TRUE,e)==2 && e[0].u.ki.wVk==VK_HANGUL);
    CHECK(surface_keyboard_events_ex(&held,0,0x71,en,TRUE,e)==2 && e[0].u.ki.wVk==VK_HANJA);
    CHECK(surface_keyboard_events_ex(&held,KBD_FLAGS_RELEASE,0x71,en,TRUE,e)==0 && held==0);
    puts("PASS: right Alt -> Hangul independent of foreground layout; repeats suppressed; release balanced across layout changes");
    puts("PASS: option OFF preserves AltGr; left Alt, right Ctrl, letters unchanged; dedicated Hangul/Hanja retained");
    puts("Actual Korean IME switching on the user's Surface remains unverified.");
    return 0;
}
