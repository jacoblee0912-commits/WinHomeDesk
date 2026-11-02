/* WinHomeDesk keyboard translation. Apache-2.0. */
#ifndef SURFACE_KEYBOARD_H
#define SURFACE_KEYBOARD_H
#include <windows.h>
#include <freerdp/input.h>

/* Korean dedicated keys can arrive as normalized 71/72 or raw F1/F2,
 * including release-only events. Emit one complete virtual-key tap so the
 * Windows IME sees a semantic key and no modifier remains held. State is
 * per connection; suppress the matching release and held-key repeats. */
static UINT surface_keyboard_events_ex(BYTE* held, UINT16 flags, BYTE code,
                                    HKL foregroundLayout, BOOL rightAltHangul, INPUT events[2]) {
    ZeroMemory(events, 2*sizeof(INPUT));
    WORD vk=0;
    if(rightAltHangul && (flags & KBD_FLAGS_EXTENDED) && code==0x38)vk=VK_HANGUL;
    else if(code==0x71 || code==0xF1)vk=VK_HANJA;
    else if(code==0x72 || code==0xF2)vk=VK_HANGUL;
    else if((flags & KBD_FLAGS_EXTENDED) && code==0x1D) {
        /* Right Alt is controlled explicitly above. With the option OFF it
         * stays a scan code, including on Korean layouts. Preserve the
         * existing Korean right Ctrl/Hanja layout mapping. */
        if(LOWORD((ULONG_PTR)foregroundLayout)==0x0412) {
            UINT mapped=MapVirtualKeyExW(0xE000|code,MAPVK_VSC_TO_VK_EX,foregroundLayout);
            if(mapped==VK_HANGUL || mapped==VK_HANJA)vk=(WORD)mapped;
        }
    }
    if(vk) {
        BYTE bit=(vk==VK_HANGUL)?1:2;
        if(flags & KBD_FLAGS_RELEASE) {
            if(*held & bit){*held &= (BYTE)~bit;return 0;}
        } else {
            if(*held & bit)return 0;
            *held |= bit;
        }
        events[0].type=events[1].type=INPUT_KEYBOARD;
        events[0].u.ki.wVk=events[1].u.ki.wVk=vk;
        events[0].u.ki.dwExtraInfo=events[1].u.ki.dwExtraInfo=0x5748444B;
        events[1].u.ki.dwFlags=KEYEVENTF_KEYUP;
        return 2;
    }
    events[0].type=INPUT_KEYBOARD;
    events[0].u.ki.wScan=code;events[0].u.ki.dwFlags=KEYEVENTF_SCANCODE;
    if(flags & KBD_FLAGS_RELEASE)events[0].u.ki.dwFlags|=KEYEVENTF_KEYUP;
    if(flags & KBD_FLAGS_EXTENDED)events[0].u.ki.dwFlags|=KEYEVENTF_EXTENDEDKEY;
    return 1;
}
/* Legacy/default-layout helper retained for tests of the original mapping. */
static UINT surface_keyboard_events(BYTE* held, UINT16 flags, BYTE code,
                                    HKL foregroundLayout, INPUT events[2]) {
    return surface_keyboard_events_ex(held,flags,code,foregroundLayout,FALSE,events);
}
#endif
