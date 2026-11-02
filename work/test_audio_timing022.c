#include "FreeRDP/server/shadow/surface_audio_timing.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){printf("FAIL line %d\n",__LINE__);return 1;}}while(0)
int main(void) {
    SurfaceAudioTiming* t=surface_audio_timing_new();CHECK(t);
    surface_audio_timing_sent(t,255,65530,1000,985);
    surface_audio_timing_confirm(t,255,65530,1002); /* receipt */
    surface_audio_timing_confirm(t,255,14,1025); /* wrap +20ms playback */
    SurfaceAudioReport r=surface_audio_timing_report(t,1040);
    CHECK(r.sent==1&&r.confirmed==1&&r.pending==0&&r.hold==20&&r.ack==25&&r.age==15);
    surface_audio_timing_confirm(t,255,65530,1041); /* duplicate must not lower hold */
    r=surface_audio_timing_report(t,1042);CHECK(r.hold==20&&r.confirmed==1);
    surface_audio_timing_sent(t,0,1042,1042,1040);
    r=surface_audio_timing_report(t,1043);CHECK(r.sent==2&&r.pending==1&&r.confirmed==1);
    surface_audio_timing_confirm(t,42,1,1050); /* unmatched ignored */
    surface_audio_timing_confirm(t,0,4,1050); /* impossible/old timestamp ignored */
    r=surface_audio_timing_report(t,1051);CHECK(r.pending==1);
    surface_audio_timing_sent(t,255,2000,2000,1999); /* reused block must clear old values */
    r=surface_audio_timing_report(t,2001);CHECK(r.confirmed==0&&r.pending==2);
    r=surface_audio_timing_report(t,5000);CHECK(r.sent==0);
    surface_audio_timing_free(t);
    puts("Audio timing PASS: timestamp wrap, duplicate confirms, missing confirms, ID reuse and expired windows");return 0;
}
