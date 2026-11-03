/* Pure PCM conversion/chunking used by the live capture and its tests. */
#pragma once
#include <AudioToolbox/AudioToolbox.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#define WHD_AUDIO_RATE 48000u
#define WHD_AUDIO_CHUNK 480u
typedef struct { int16_t samples[WHD_AUDIO_CHUNK*2]; unsigned pending; uint64_t tick; } WHDPCM;
typedef void (*WHDPCMSink)(void*,const int16_t*,unsigned,uint64_t);
static int whd_pcm_feed(WHDPCM* p,const AudioStreamBasicDescription* f,const AudioBufferList* b,
                       unsigned frames,uint64_t tick,void* context,WHDPCMSink sink) {
    if(!p||!f||!b||!sink||f->mFormatID!=kAudioFormatLinearPCM || f->mSampleRate!=WHD_AUDIO_RATE ||
       f->mChannelsPerFrame!=2 || (f->mFormatFlags&kAudioFormatFlagIsBigEndian)) return 0;
    int floating=(f->mFormatFlags&kAudioFormatFlagIsFloat)!=0;
    int planar=(f->mFormatFlags&kAudioFormatFlagIsNonInterleaved)!=0;
    unsigned bytes=floating?4:2, buffers=planar?2:1;
    if(f->mBitsPerChannel!=bytes*8 || (!floating && !(f->mFormatFlags&kAudioFormatFlagIsSignedInteger)) ||
       f->mBytesPerFrame!=bytes*(planar?1:2) || b->mNumberBuffers!=buffers) return 0;
    for(unsigned i=0;i<buffers;i++)
        if(!b->mBuffers[i].mData || b->mBuffers[i].mNumberChannels!=(planar?1:2) ||
           (uint64_t)frames*f->mBytesPerFrame>b->mBuffers[i].mDataByteSize) return 0;
    if(p->pending && (tick>p->tick+30 || tick+10<p->tick)) p->pending=0;
    for(unsigned i=0;i<frames;i++) {
        if(!p->pending)p->tick=tick+(uint64_t)i*1000/WHD_AUDIO_RATE;
        for(unsigned ch=0;ch<2;ch++) {
            const uint8_t* src=(const uint8_t*)b->mBuffers[planar?ch:0].mData+
                ((size_t)i*(planar?1:2)+(planar?0:ch))*bytes;
            int16_t value;
            if(floating) {
                float v;memcpy(&v,src,4); if(!isfinite(v))v=0;
                value=(int16_t)lrintf(fmaxf(-1,fminf(1,v))*32767);
            } else memcpy(&value,src,2);
            p->samples[p->pending*2+ch]=value;
        }
        if(++p->pending==WHD_AUDIO_CHUNK) {sink(context,p->samples,WHD_AUDIO_CHUNK,p->tick);p->pending=0;}
    }
    return 1;
}
