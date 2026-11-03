#import "mac_audio.h"
#import <ApplicationServices/ApplicationServices.h>
#include "mac_audio_pcm.h"
#include <winpr/synch.h>
#include <winpr/sysinfo.h>
#include <winpr/interlocked.h>
#include <winpr/collections.h>
static AUDIO_FORMAT format={.wFormatTag=1,.nChannels=2,.nSamplesPerSec=WHD_AUDIO_RATE,
    .nAvgBytesPerSec=WHD_AUDIO_RATE*4,.nBlockAlign=4,.wBitsPerSample=16};
@interface WHDAudio () {
    WHDPCM _pcm;
    uint64_t _queued,_dropped,_frames,_report,_testFrame;
    unsigned _peak;
}
@property(nonatomic) rdpShadowSubsystem* base;
@property(nonatomic,strong) SCStream* stream;
@property(nonatomic,strong) dispatch_queue_t queue;
@property(atomic) BOOL cancelled;
@end
static void free_audio(UINT32 id,SHADOW_MSG_OUT* packet) {free(packet);}
static void send_pcm(void* context,const int16_t* samples,unsigned frames,uint64_t tick);
@implementation WHDAudio
- (instancetype)initWithSubsystem:(rdpShadowSubsystem*)base {
    if((self=[super init])) {
        _base=base;_queue=dispatch_queue_create("WinHomeDesk.audio",DISPATCH_QUEUE_SERIAL);
        base->rdpsndFormats=&format;base->nRdpsndFormats=1;
    } return self;
}
- (void)send:(const int16_t*)samples frames:(unsigned)frames tick:(uint64_t)tick {
    uint64_t now=GetTickCount64();
    if(self.cancelled || WaitForSingleObject(_base->server->StopEvent,0)==WAIT_OBJECT_0)return;
    if(now>tick+80) {_dropped++;return;}
    _frames+=frames;
    for(unsigned i=0;i<frames*2;i++) {unsigned v=abs(samples[i]);if(v>_peak)_peak=v;}
    rdpShadowServer* server=_base->server;
    ArrayList_Lock(server->clients);
    for(size_t i=0;i<ArrayList_Count(server->clients);i++) {
        rdpShadowClient* c=ArrayList_GetItem(server->clients,i);
        if(!c->activated || !c->mayView || c->inLobby || !InterlockedCompareExchange(&c->surfaceAudioReady,0,0))continue;
        if(MessageQueue_Size(c->MsgQueue)>=8){_dropped++;continue;}
        SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES* p=calloc(1,sizeof(*p)+(size_t)frames*4);
        if(!p){_dropped++;continue;}
        p->common.Free=free_audio;p->audio_format=&format;p->buf=p+1;p->nFrames=frames;
        p->surfaceCapturedAt=tick;p->wTimestamp=(UINT16)tick;memcpy(p->buf,samples,(size_t)frames*4);
        if(shadow_client_post_msg(c,NULL,SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES_ID,(SHADOW_MSG_OUT*)p,NULL))_queued++;else _dropped++;
    }
    ArrayList_Unlock(server->clients);
    if(now-_report>=2000) {
        fprintf(stderr,"WinHomeDesk AUDIO_STATS frames=%llu queued=%llu dropped=%llu peak=%u chunk_ms=10 queue_limit=8\n",
            (unsigned long long)_frames,(unsigned long long)_queued,(unsigned long long)_dropped,_peak);
        _frames=_queued=_dropped=0;_peak=0;_report=now;
    }
}
- (void)stream:(SCStream*)stream didOutputSampleBuffer:(CMSampleBufferRef)sample ofType:(SCStreamOutputType)type {
    if(self.cancelled || type!=SCStreamOutputTypeAudio || !CMSampleBufferIsValid(sample))return;
    CFDictionaryRef session=CGSessionCopyCurrentDictionary();
    BOOL allowed=session && CFDictionaryGetValue(session,kCGSessionOnConsoleKey)==kCFBooleanTrue &&
        CFDictionaryGetValue(session,CFSTR("CGSSessionScreenIsLocked"))!=kCFBooleanTrue;
    if(session)CFRelease(session);
    if(!allowed){_pcm.pending=0;return;}
    const AudioStreamBasicDescription* f=CMAudioFormatDescriptionGetStreamBasicDescription(CMSampleBufferGetFormatDescription(sample));
    CMItemCount count=CMSampleBufferGetNumSamples(sample);
    if(!f || count<=0 || count>16384)return;
    struct {AudioBufferList list;AudioBuffer extra;} storage={0};
    CMBlockBufferRef retained=NULL;
    OSStatus status=CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(sample,NULL,&storage.list,sizeof(storage),NULL,NULL,
        kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment,&retained);
    if(status)return;
    double age=CMTimeGetSeconds(CMTimeSubtract(CMClockGetTime(CMClockGetHostTimeClock()),CMSampleBufferGetPresentationTimeStamp(sample)));
    uint64_t now=GetTickCount64();
    if(!isfinite(age)||age<-.01||age>.08){_dropped++;_pcm.pending=0;}
    else if(!whd_pcm_feed(&_pcm,f,&storage.list,(unsigned)count,now-(uint64_t)(fmax(0,age)*1000),(__bridge void*)self,send_pcm)) {
        _dropped++;_pcm.pending=0;
        if(!_report){fprintf(stderr,"WinHomeDesk AUDIO unsupported PCM rate=%.0f flags=%u bits=%u\n",f->mSampleRate,f->mFormatFlags,f->mBitsPerChannel);_report=now;}
    }
    CFRelease(retained);
}
- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error {
    fprintf(stderr,"WinHomeDesk AUDIO state=error message=%s\n",error.localizedDescription.UTF8String);
}
- (BOOL)startWithFilter:(SCContentFilter*)filter {
    SCStreamConfiguration* c=[SCStreamConfiguration new];
    c.width=16;c.height=16;c.minimumFrameInterval=CMTimeMake(1,1);c.queueDepth=3;c.showsCursor=NO;
    c.capturesAudio=YES;c.sampleRate=WHD_AUDIO_RATE;c.channelCount=2;c.captureMicrophone=NO;
    self.stream=[[SCStream alloc] initWithFilter:filter configuration:c delegate:self];
    NSError* error=nil;
    if(![self.stream addStreamOutput:self type:SCStreamOutputTypeAudio sampleHandlerQueue:self.queue error:&error])return NO;
    dispatch_semaphore_t ready=dispatch_semaphore_create(0);__block BOOL started=NO;
    [self.stream startCaptureWithCompletionHandler:^(NSError* e){started=!e;dispatch_semaphore_signal(ready);}];
    if(dispatch_semaphore_wait(ready,dispatch_time(DISPATCH_TIME_NOW,10*NSEC_PER_SEC)))return NO;
    fprintf(stderr,"WinHomeDesk AUDIO state=%s backend=ScreenCaptureKit rate=48000 channels=2 bits=16 chunk_ms=10\n",started?"capturing":"error");
    return started;
}
- (void)stop {
    self.cancelled=YES;
    if(self.stream) {
        dispatch_semaphore_t done=dispatch_semaphore_create(0);
        [self.stream stopCaptureWithCompletionHandler:^(NSError* e){dispatch_semaphore_signal(done);}];
        dispatch_semaphore_wait(done,dispatch_time(DISPATCH_TIME_NOW,5*NSEC_PER_SEC));
        [self.stream removeStreamOutput:self type:SCStreamOutputTypeAudio error:NULL];
        self.stream=nil;
    }
    dispatch_sync(self.queue, ^{});
}
- (void)syntheticTick {
    /* In-memory test tone, never sent to a local speaker or written to disk. */
    int16_t samples[WHD_AUDIO_CHUNK*2];
    for(unsigned i=0;i<WHD_AUDIO_CHUNK;i++) {
        samples[i*2]=samples[i*2+1]=(int16_t)(4000*sin(2*M_PI*440*(_testFrame++)/WHD_AUDIO_RATE));
    }
    [self send:samples frames:WHD_AUDIO_CHUNK tick:GetTickCount64()];
}
@end
static void send_pcm(void* context,const int16_t* samples,unsigned frames,uint64_t tick) {
    [(__bridge WHDAudio*)context send:samples frames:frames tick:tick];
}
