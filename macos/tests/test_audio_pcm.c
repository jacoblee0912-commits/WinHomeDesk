#include "../../work/FreeRDP/server/shadow/Mac/mac_audio_pcm.h"
#include <assert.h>
#include <stdio.h>
static unsigned packets;static int16_t saved[960];static uint64_t stamp;
static void sink(void* c,const int16_t* data,unsigned n,uint64_t tick){assert(n==480);memcpy(saved,data,sizeof(saved));packets++;stamp=tick;}
int main(void) {
    WHDPCM pcm={0};float l[480],r[480];for(unsigned i=0;i<480;i++){l[i]=.25f;r[i]=-.5f;}
    struct {AudioBufferList b;AudioBuffer extra;} list={.b.mNumberBuffers=2};
    list.b.mBuffers[0]=(AudioBuffer){1,sizeof(l),l};list.b.mBuffers[1]=(AudioBuffer){1,sizeof(r),r};
    AudioStreamBasicDescription f={.mSampleRate=48000,.mFormatID=kAudioFormatLinearPCM,
        .mFormatFlags=kAudioFormatFlagIsFloat|kAudioFormatFlagIsNonInterleaved,.mBytesPerFrame=4,.mChannelsPerFrame=2,.mBitsPerChannel=32};
    assert(whd_pcm_feed(&pcm,&f,&list.b,240,100,NULL,sink)&&packets==0);
    assert(whd_pcm_feed(&pcm,&f,&list.b,240,105,NULL,sink)&&packets==1&&stamp==100);
    assert(saved[0]==8192&&saved[1]==-16384);
    l[0]=NAN;l[1]=INFINITY;l[2]=2;r[2]=-2;
    assert(whd_pcm_feed(&pcm,&f,&list.b,480,110,NULL,sink));
    assert(saved[0]==0&&saved[2]==0&&saved[4]==32767&&saved[5]==-32767);
    list.b.mBuffers[1].mDataByteSize=4;assert(!whd_pcm_feed(&pcm,&f,&list.b,480,120,NULL,sink));
    list.b.mBuffers[1].mDataByteSize=sizeof(r);f.mSampleRate=44100;
    assert(!whd_pcm_feed(&pcm,&f,&list.b,480,120,NULL,sink));f.mSampleRate=48000;
    f.mFormatFlags|=kAudioFormatFlagIsBigEndian;assert(!whd_pcm_feed(&pcm,&f,&list.b,480,120,NULL,sink));
    f.mFormatFlags=kAudioFormatFlagIsSignedInteger;f.mBitsPerChannel=16;
    int16_t interleaved[960];for(unsigned i=0;i<960;i++)interleaved[i]=(int16_t)(i-480);
    list.b.mNumberBuffers=1;list.b.mBuffers[0]=(AudioBuffer){2,sizeof(interleaved),interleaved};
    assert(whd_pcm_feed(&pcm,&f,&list.b,480,120,NULL,sink));assert(!memcmp(saved,interleaved,sizeof(saved)));
    assert(whd_pcm_feed(&pcm,&f,&list.b,100,130,NULL,sink)&&pcm.pending==100);
    unsigned before=packets;
    assert(whd_pcm_feed(&pcm,&f,&list.b,480,300,NULL,sink)&&packets==before+1&&pcm.pending==0&&stamp==300);
    puts("PASS audio: planar/interleaved, PCM16/float32, stereo, clipping/NaN, bounds, rate, discontinuity, 10ms chunks");
}
