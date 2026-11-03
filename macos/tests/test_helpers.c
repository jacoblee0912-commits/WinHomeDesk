#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "../../work/FreeRDP/server/shadow/Mac/mac_helpers.h"
#include <winpr/input.h>
int main(void) {
    uint8_t bytes[2];
    assert(whd_hex("aB01",bytes,2) && bytes[0]==171 && bytes[1]==1);
    assert(!whd_hex("aZ01",bytes,2)); assert(!whd_hex("a001x",bytes,2)); assert(!whd_hex(NULL,bytes,2));
    assert(whd_wheel(0x0278)==120); assert(whd_wheel(0x0388)==-120);
    assert(whd_wheel(0x0301)==-255); assert(whd_wheel(0x02ff)==255);
    assert(whd_coordinate(1919,1920,-1440,1440)==-1);
    assert(whd_coordinate(65535,1920,0,2560)==2559);
    assert(whd_coordinate(0,1,10,100)==10);
    uint8_t ime=0;
    assert(whd_hangul_event(&ime,0x72,false,false,false)==WHD_IME_TOGGLE);
    assert(whd_hangul_event(&ime,0x72,false,false,false)==WHD_IME_CONSUME);
    assert(whd_hangul_event(&ime,0x72,false,true,false)==WHD_IME_CONSUME && !ime);
    assert(whd_hangul_event(&ime,0xf2,false,true,false)==WHD_IME_TOGGLE && !ime);
    assert(whd_hangul_event(&ime,0x72,true,true,false)==WHD_IME_TOGGLE);
    assert(whd_hangul_event(&ime,0xf2,false,false,false)==WHD_IME_TOGGLE);
    assert(whd_hangul_event(&ime,0x72,false,true,false)==WHD_IME_CONSUME && !ime);
    assert(whd_hangul_event(&ime,0x38,true,false,true)==WHD_IME_TOGGLE);
    assert(whd_hangul_event(&ime,0x38,true,true,true)==WHD_IME_CONSUME && !ime);
    assert(whd_hangul_event(&ime,0x38,true,true,true)==WHD_IME_TOGGLE);
    assert(whd_hangul_event(&ime,0x38,true,false,false)==WHD_IME_NONE);
    assert(whd_hangul_event(&ime,0x38,false,false,true)==WHD_IME_NONE);
    assert(whd_hangul_event(&ime,0x71,false,false,true)==WHD_IME_NONE);
    assert(whd_hangul_event(&ime,0x0e,false,false,true)==WHD_IME_NONE);
    const unsigned scans[]={0x0e,0x1e,0x2a,0x36,0x1d,0x11d,0x138,0x15b,0x14b,0x153};
    const unsigned keys[]={51,0,56,60,59,62,61,55,123,117};
    for(unsigned i=0;i<sizeof(scans)/sizeof(scans[0]);i++) {
        unsigned vk=GetVirtualKeyCodeFromVirtualScanCode(scans[i],4);
        if(scans[i]&0x100) vk|=KBDEXT;
        unsigned key=GetKeycodeFromVirtualKeyCode(vk,WINPR_KEYCODE_TYPE_APPLE);
        if(key!=keys[i]) { fprintf(stderr,"scan %x key %u expected %u\n",scans[i],key,keys[i]); return 1; }
        unsigned outgoing=GetVirtualScanCodeFromVirtualKeyCode(GetVirtualKeyCodeFromKeycode(key,WINPR_KEYCODE_TYPE_APPLE),4);
        if(outgoing!=scans[i]) { fprintf(stderr,"key %u scan %x expected %x\n",key,outgoing,scans[i]); return 1; }
    }
    puts("PASS: strict credential hex, signed wheel, Retina coordinates, key mapping, Hangul scan transitions");
}
