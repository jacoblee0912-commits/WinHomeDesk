/* WinHomeDesk native macOS RDP client. Apache-2.0. */
#import "Client.h"
#include <winpr/sysinfo.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/gfx.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/channels.h>
#include <freerdp/addin.h>
#include <freerdp/input.h>
#include <freerdp/error.h>
#include <freerdp/codec/color.h>
#include <winpr/input.h>
#include <winpr/synch.h>

typedef struct { rdpContext base; void *owner; BOOL dirty; pcRdpgfxResetGraphics resetGraphics; } WHDContext;
static WHDClient *owner(rdpContext *c) { return (__bridge WHDClient *)((WHDContext *)c)->owner; }
static BOOL remote_monitors(rdpContext* c,UINT32 count,const MONITOR_DEF* monitors) {return [owner(c) setRemoteMonitors:monitors count:count];}
static UINT reset_graphics(RdpgfxClientContext* context,const RDPGFX_RESET_GRAPHICS_PDU* pdu) {
    rdpGdi* gdi=context->custom;WHDContext* c=(WHDContext*)gdi->context;
    UINT result=c->resetGraphics(context,pdu);
    if(result==CHANNEL_RC_OK && !remote_monitors(&c->base,pdu->monitorCount,pdu->monitorDefArray))return ERROR_INVALID_DATA;
    return result;
}
static BOOL begin(rdpContext *c) { c->gdi->primary->hdc->hwnd->invalid->null = TRUE; return TRUE; }
static BOOL end(rdpContext *c) {
    if (c->gdi->primary->hdc->hwnd->invalid->null) return TRUE;
    WHDClient *client = owner(c); rdpGdi *g = c->gdi;
    if (client.loopbackTest && !client.captureTest && g->width == 800 && g->height == 600) {
        const BYTE *p = g->primary_buffer + 29 * g->stride + 37 * 4;
        const BYTE *q = g->primary_buffer + 101 * g->stride + 191 * 4;
        int tolerance=client.graphicsMode<2?8:0;
        if (abs((int)p[0]-37)<=tolerance && abs((int)p[1]-29)<=tolerance && abs((int)q[0]-191)<=tolerance && abs((int)q[1]-101)<=tolerance) client.patternFrames++;
    }
    client.desktopSize=NSMakeSize(g->width,g->height); client.frames++;
    ((WHDContext *)c)->dirty=TRUE;
    return TRUE;
}
/* Bitmap PDUs may call EndPaint hundreds of times per video frame. Snapshot
 * once after processing pending network events, never once per rectangle. */
static void present(rdpContext *c) {
    WHDContext *context=(WHDContext *)c;
    WHDClient *client=owner(c); rdpGdi *g=c->gdi;
    if(!context->dirty || client.probe || !g) return;
    context->dirty=FALSE;
    CGColorSpaceRef color = CGColorSpaceCreateDeviceRGB();
    CFDataRef data = CFDataCreate(NULL, g->primary_buffer, g->stride * g->height);
    CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
    CGImageRef cg = CGImageCreate(g->width,g->height,8,32,g->stride,color,
        kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst, provider,NULL,false,kCGRenderingIntentDefault);
    if (cg) { client.image = [[NSImage alloc] initWithCGImage:cg size:NSMakeSize(g->width,g->height)]; CGImageRelease(cg); }
    CGDataProviderRelease(provider); CFRelease(data); CGColorSpaceRelease(color);
 }
static BOOL resize(rdpContext *c) {
    return gdi_resize(c->gdi, freerdp_settings_get_uint32(c->settings,FreeRDP_DesktopWidth),
                      freerdp_settings_get_uint32(c->settings,FreeRDP_DesktopHeight));
}
static void channel_connected(void *context, const ChannelConnectedEventArgs *e) {
    rdpContext *c=context;
    if(!strcmp(e->name,RDPGFX_DVC_CHANNEL_NAME)) {
        owner(c).gfxConnected=gdi_graphics_pipeline_init(c->gdi,(RdpgfxClientContext *)e->pInterface);
        if(owner(c).gfxConnected){RdpgfxClientContext* gfx=e->pInterface;((WHDContext*)c)->resetGraphics=gfx->ResetGraphics;gfx->ResetGraphics=reset_graphics;}
        fprintf(stderr,"WinHomeDesk CLIENT graphics=gfx initialized=%d\n",owner(c).gfxConnected);
    }
}
static void channel_disconnected(void *context, const ChannelDisconnectedEventArgs *e) {
    rdpContext *c=context;
    if(!strcmp(e->name,RDPGFX_DVC_CHANNEL_NAME) && c->gdi)
        gdi_graphics_pipeline_uninit(c->gdi,(RdpgfxClientContext *)e->pInterface);
}
static BOOL load_channels(freerdp *rdp) {
    return freerdp_client_load_addins(rdp->context->channels,rdp->context->settings);
}
static BOOL pre_connect(freerdp *rdp) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{ freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry,0); });
    rdp->context->update->RemoteMonitors=remote_monitors;
    if(PubSub_SubscribeChannelConnected(rdp->context->pubSub,channel_connected)<0 ||
       PubSub_SubscribeChannelDisconnected(rdp->context->pubSub,channel_disconnected)<0) return FALSE;
    return TRUE;
}
static BOOL post_connect(freerdp *rdp) {
    if (!gdi_init(rdp,PIXEL_FORMAT_BGRX32)) return FALSE;
    rdp->context->update->BeginPaint=begin; rdp->context->update->EndPaint=end;
    rdp->context->update->DesktopResize=resize;
    WHDClient *client=owner(rdp->context); client.connected=YES; client.status=@"연결됨 / Connected";
    return TRUE;
}
static DWORD verify(freerdp *rdp, const char *host, UINT16 port, const char *cn,
                    const char *subject, const char *issuer, const char *fingerprint, DWORD flags) {
    WHDClient *client=owner(rdp->context);
    if (client.loopbackTest && strcmp(host,"127.0.0.1")==0) return 2;
    if (client.probe || client.stop) return 0;
    __block DWORD result=0;
    dispatch_sync(dispatch_get_main_queue(), ^{
        NSAlert *a=[NSAlert new]; a.messageText=@"서버 인증서 확인 / Verify server certificate";
        a.informativeText=[NSString stringWithFormat:@"%s:%u\n%s\nFingerprint: %s\n\n서버 담당자와 지문을 확인한 경우에만 연결하세요. 이번 연결에만 허용합니다.",host,port,subject,fingerprint];
        [a addButtonWithTitle:@"취소 / Cancel"]; [a addButtonWithTitle:@"이번 연결 허용 / Allow once"];
        if ([a runModal]==NSAlertSecondButtonReturn) result=2;
    });
    return result;
}
static DWORD changed(freerdp *rdp, const char *host, UINT16 port, const char *cn,
    const char *subject,const char *issuer,const char *fingerprint,const char *oldSubject,
    const char *oldIssuer,const char *oldFingerprint,DWORD flags) {
    /* A changed stored identity is never silently replaced. */
    owner(rdp->context).status=@"인증서 변경: 연결 거부 / Certificate changed";
    return 0;
}
@implementation WHDClient
- (BOOL)setRemoteMonitors:(const MONITOR_DEF*)monitors count:(UINT32)count {
    if(!monitors || !count || count>16)return NO;
    int64_t left=INT32_MAX,top=INT32_MAX,right=INT32_MIN,bottom=INT32_MIN;unsigned primary=0;
    for(UINT32 i=0;i<count;i++) {
        const MONITOR_DEF* m=&monitors[i];
        if(m->right<m->left || m->bottom<m->top)return NO;
        left=MIN(left,m->left);top=MIN(top,m->top);right=MAX(right,m->right);bottom=MAX(bottom,m->bottom);
        primary+=(m->flags&1)!=0;
    }
    if(primary!=1 || right-left+1>65535 || bottom-top+1>65535)return NO;
    NSMutableArray* result=[NSMutableArray new];
    for(UINT32 i=0;i<count;i++) {
        const MONITOR_DEF* m=&monitors[i];
        NSRect rect=NSMakeRect(m->left-left,m->top-top,(int64_t)m->right-m->left+1,(int64_t)m->bottom-m->top+1);
        [result addObject:@{@"rect":[NSValue valueWithRect:rect],@"primary":@((m->flags&1)!=0)}];
    }
    self.monitors=result;
    fprintf(stderr,"WinHomeDesk CLIENT monitors=%u desktop=%lldx%lld\n",count,(long long)(right-left+1),(long long)(bottom-top+1));
    return YES;
}
- (instancetype)init { if((self=[super init])) { _events=[NSMutableArray new]; _status=@"연결 대기"; _inputWake=CreateEvent(NULL,TRUE,FALSE,NULL); } return self; }
- (void)dealloc { if(_inputWake)CloseHandle(_inputWake); }
- (void)enqueue:(void (^)(rdpInput *))event {
    if (!self.connected || self.stop) return;
    @synchronized(self.events) { self.lastMovement=nil; if(self.events.count < 4096) [self.events addObject:[event copy]]; else self.stop=YES; SetEvent(self.inputWake); }
}
- (void)enqueueMovement:(void (^)(rdpInput*))event {
    if(!self.connected || self.stop)return;
    @synchronized(self.events) {
        if(self.lastMovement && self.events.lastObject==self.lastMovement)[self.events removeLastObject];
        if(self.events.count<4096){self.lastMovement=event;[self.events addObject:self.lastMovement];}else self.stop=YES;
        SetEvent(self.inputWake);
    }
}
- (void)connectHost:(NSString *)host port:(int)port user:(NSString *)user domain:(NSString *)domain password:(NSString *)password {
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0), ^{
        @autoreleasepool {
            freerdp *rdp=freerdp_new();
            if(!rdp) { self.status=@"메모리 할당 실패"; self.finished=YES; return; }
            rdp->ContextSize=sizeof(WHDContext);
            if(!freerdp_context_new(rdp)) { freerdp_free(rdp); self.finished=YES; return; }
            ((WHDContext *)rdp->context)->owner=(__bridge void *)self;
            rdp->LoadChannels=load_channels; rdp->PreConnect=pre_connect; rdp->PostConnect=post_connect; rdp->VerifyCertificateEx=verify; rdp->VerifyChangedCertificateEx=changed;
            rdpSettings *settings=rdp->context->settings;
            BOOL configured=freerdp_settings_set_string(settings,FreeRDP_ServerHostname,host.UTF8String) &&
              freerdp_settings_set_uint32(settings,FreeRDP_ServerPort,port) &&
              freerdp_settings_set_string(settings,FreeRDP_Username,user.UTF8String) &&
              freerdp_settings_set_string(settings,FreeRDP_Domain,domain.UTF8String) &&
              freerdp_settings_set_string(settings,FreeRDP_Password,password.UTF8String) &&
              freerdp_settings_set_uint32(settings,FreeRDP_DesktopWidth,1280) &&
              freerdp_settings_set_uint32(settings,FreeRDP_DesktopHeight,800) &&
              freerdp_settings_set_uint32(settings,FreeRDP_ColorDepth,32) &&
              freerdp_settings_set_uint32(settings,FreeRDP_TcpConnectTimeout,10000) &&
              freerdp_settings_set_bool(settings,FreeRDP_SupportMonitorLayoutPdu,TRUE) &&
              freerdp_settings_set_bool(settings,FreeRDP_SupportGraphicsPipeline,self.graphicsMode!=3) &&
              freerdp_settings_set_bool(settings,FreeRDP_GfxH264,self.graphicsMode<2) &&
              freerdp_settings_set_bool(settings,FreeRDP_GfxAVC444,self.graphicsMode==1) &&
              freerdp_settings_set_bool(settings,FreeRDP_GfxAVC444v2,self.graphicsMode==1) &&
              freerdp_settings_set_bool(settings,FreeRDP_GfxProgressive,FALSE) &&
              freerdp_settings_set_bool(settings,FreeRDP_GfxProgressiveV2,FALSE) &&
              freerdp_settings_set_bool(settings,FreeRDP_RedirectClipboard,FALSE) &&
              freerdp_settings_set_bool(settings,FreeRDP_AudioPlayback,self.audioProbe && self.probe && self.loopbackTest) &&
              freerdp_settings_set_bool(settings,FreeRDP_AudioCapture,FALSE) &&
              freerdp_settings_set_bool(settings,FreeRDP_RemoteFxCodec,FALSE) &&
              freerdp_settings_set_bool(settings,FreeRDP_NSCodec,FALSE) &&
              freerdp_settings_set_bool(settings,FreeRDP_NlaSecurity,!self.tlsOnly) &&
              freerdp_settings_set_bool(settings,FreeRDP_TlsSecurity,TRUE) &&
              freerdp_settings_set_bool(settings,FreeRDP_RdpSecurity,FALSE);
            if(self.audioProbe && self.probe && self.loopbackTest) {
                const char* args[]={"rdpsnd","sys:fake","latency:10"};
                configured=configured && freerdp_client_add_static_channel(settings,3,args);
            }
            self.status=@"연결 중 / Connecting";
            BOOL ok=configured && freerdp_connect(rdp);
            if (!ok) {
                UINT32 error=freerdp_get_last_error(rdp->context);
                NSString *detail=@"RDP 연결 실패";
                if(error==FREERDP_ERROR_CONNECT_FAILED) detail=@"TCP 연결 실패: 주소·포트·로컬 네트워크 권한 확인 후 접속 진단";
                else if(error==FREERDP_ERROR_AUTHENTICATION_FAILED || error==FREERDP_ERROR_CONNECT_LOGON_FAILURE)
                    detail=@"인증 실패: Windows 로그인 암호가 아닌 WinHomeDesk 앱 암호인지 확인";
                else if(error==FREERDP_ERROR_TLS_CONNECT_FAILED) detail=@"TLS 연결 실패: 서버 인증서 확인";
                else if(error==FREERDP_ERROR_CONNECT_TRANSPORT_FAILED) detail=@"RDP 통신 실패: 서버의 동시 접속 제한·서버 로그·네트워크 확인";
                else if(error==FREERDP_ERROR_CONNECT_CANCELLED) detail=@"연결 취소됨";
                self.status=[NSString stringWithFormat:@"%@ (0x%08x)",detail,error];
            }
            ULONGLONG connectedAt=GetTickCount64();
            BOOL sentTestInput=NO;
            ULONGLONG sentTestInputAt=0;
            while(ok && !self.stop && !freerdp_shall_disconnect_context(rdp->context)) {
                @autoreleasepool {
            if (!sentTestInput && self.probe && self.loopbackTest && !self.captureTest && self.patternFrames > 0) {
                sentTestInput=YES;
                sentTestInputAt=GetTickCount64();
                rdpInput *input=rdp->context->input;
                freerdp_input_send_keyboard_event_ex(input,TRUE,FALSE,0x1e);
                freerdp_input_send_keyboard_event_ex(input,FALSE,FALSE,0x1e);
                freerdp_input_send_keyboard_event_ex(input,TRUE,FALSE,0x72);
                freerdp_input_send_keyboard_event_ex(input,TRUE,FALSE,0x72); // held-key repeat
                freerdp_input_send_keyboard_event_ex(input,FALSE,FALSE,0x72);
                freerdp_input_send_keyboard_event(input,KBD_FLAGS_RELEASE,0xf2); // raw release-only
                freerdp_input_send_unicode_keyboard_event(input,0,0xD55C);
                freerdp_input_send_unicode_keyboard_event(input,KBD_FLAGS_RELEASE,0xD55C);
                freerdp_input_send_mouse_event(input,PTR_FLAGS_MOVE,100,100);
                freerdp_input_send_mouse_event(input,PTR_FLAGS_BUTTON1|PTR_FLAGS_DOWN,100,100);
                freerdp_input_send_mouse_event(input,PTR_FLAGS_BUTTON1,100,100);
                freerdp_input_send_mouse_event(input,PTR_FLAGS_WHEEL|0x188,0,0);
            }
                    NSArray *pending;
                    @synchronized(self.events) { ResetEvent(self.inputWake); pending=[self.events copy]; [self.events removeAllObjects]; self.lastMovement=nil; }
                    for (void (^event)(rdpInput *) in pending) event(rdp->context->input);
                    HANDLE handles[MAXIMUM_WAIT_OBJECTS];
                    DWORD count=freerdp_get_event_handles(rdp->context,handles,MAXIMUM_WAIT_OBJECTS-1);
                    handles[count]=self.inputWake;
                    if(!count || !self.inputWake || WaitForMultipleObjects(count+1,handles,FALSE,20)==WAIT_FAILED || !freerdp_check_event_handles(rdp->context)) break;
                    present(rdp->context);
                    if(self.probe && (self.loopbackTest && !self.captureTest ?
                        (self.patternFrames>=3 && sentTestInput && GetTickCount64()-sentTestInputAt>=(self.audioProbe?2500:250)) : (self.captureTest ? self.frames>=1 && GetTickCount64()-connectedAt>=2500 : self.frames>=30))) break;
                }
            }
            self.connected=NO;
            freerdp_disconnect(rdp); gdi_free(rdp);
            freerdp_context_free(rdp); freerdp_free(rdp);
            if(ok) self.status=@"연결 종료 / Disconnected";
            self.finished=YES;
        }
    });
}
@end

@interface WHDRemoteView ()
@property(nonatomic) BOOL composingDeletion;
@property NSImage* cropSource;
@property NSImage* cropImage;
@property NSRect cropRect;
@property UINT16 heldButtons;
@property NSPoint lastPointer;
@end
@implementation WHDRemoteView
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (instancetype)initWithFrame:(NSRect)frame {
    if((self=[super initWithFrame:frame])) { _held=[NSMutableSet new]; _marked=[NSMutableAttributedString new];_monitorIndex=-1; } return self;
}
- (NSRect)sourceRect {
    NSSize size=self.client.desktopSize;NSRect desktop=NSMakeRect(0,0,size.width,size.height);
    if(self.monitorIndex<0)return desktop;
    NSArray* monitors=self.client.monitors;
    if(!monitors.count)return self.monitorIndex==0?desktop:NSZeroRect;
    if(self.monitorIndex>=monitors.count)return NSZeroRect;
    NSRect rect=[monitors[self.monitorIndex][@"rect"] rectValue];
    return NSContainsRect(desktop,rect)?rect:NSZeroRect;
}
- (NSRect)imageRect {
    NSSize size=[self sourceRect].size;
    if(size.width<1 || size.height<1) return self.bounds;
    CGFloat ratio=MIN(self.bounds.size.width/size.width,self.bounds.size.height/size.height);
    NSSize target=NSMakeSize(size.width*ratio,size.height*ratio);
    return NSMakeRect((self.bounds.size.width-target.width)/2,(self.bounds.size.height-target.height)/2,target.width,target.height);
}
- (void)drawRect:(NSRect)rect {
    [NSColor.blackColor setFill]; NSRectFill(self.bounds);
    NSImage* image=self.client.image;NSRect source=[self sourceRect];
    if(!image || NSIsEmptyRect(source))return;
    if(self.cropSource!=image || !NSEqualRects(self.cropRect,source)) {
        self.cropSource=image;self.cropRect=source;self.cropImage=nil;
        CGImageRef full=[image CGImageForProposedRect:NULL context:nil hints:nil];
        CGImageRef crop=full?CGImageCreateWithImageInRect(full,NSRectToCGRect(source)):NULL;
        if(crop){self.cropImage=[[NSImage alloc] initWithCGImage:crop size:source.size];CGImageRelease(crop);}
    }
    [self.cropImage drawInRect:[self imageRect] fromRect:NSZeroRect operation:NSCompositingOperationCopy fraction:1 respectFlipped:YES hints:nil];
}
- (void)sendKey:(unsigned)key down:(BOOL)down {
    DWORD vk=GetVirtualKeyCodeFromKeycode(key,WINPR_KEYCODE_TYPE_APPLE);
    DWORD scan=GetVirtualScanCodeFromVirtualKeyCode(vk,4);
    if(!scan) return;
    [self.client enqueue:^(rdpInput *input) { freerdp_input_send_keyboard_event_ex(input,down,FALSE,scan); }];
}
- (void)keyDown:(NSEvent *)event {
    /* Local IME commits Unicode; shortcuts and navigation remain physical keys. */
    NSString *chars=event.characters;
    BOOL shortcut=(event.modifierFlags & (NSEventModifierFlagControl|NSEventModifierFlagCommand))!=0;
    BOOL backspace=event.keyCode==0x33; // macOS Delete/Backspace produces 0x7F, not 0x08.
    if(backspace && self.hasMarkedText && !shortcut) {
        self.composingDeletion=YES;
        @try { [self interpretKeyEvents:@[event]]; }
        @finally { self.composingDeletion=NO; }
        return;
    }
    BOOL physical=shortcut || backspace || !chars.length ||
        [chars characterAtIndex:0]<32 || [chars characterAtIndex:0]>=0xF700;
    if(physical) { [self.held addObject:@(event.keyCode)]; [self sendKey:event.keyCode down:YES]; }
    else [self interpretKeyEvents:@[event]];
}
- (void)keyUp:(NSEvent *)event {
    if([self.held containsObject:@(event.keyCode)]) { [self sendKey:event.keyCode down:NO]; [self.held removeObject:@(event.keyCode)]; }
}
- (void)flagsChanged:(NSEvent *)e {
    NSUInteger mask=0;
    switch(e.keyCode) {
        case 56: case 60: mask=NSEventModifierFlagShift; break;
        case 59: case 62: mask=NSEventModifierFlagControl; break;
        case 58: case 61: mask=NSEventModifierFlagOption; break;
        case 54: case 55: mask=NSEventModifierFlagCommand; break;
        case 57: {
            UINT32 flags=(e.modifierFlags & NSEventModifierFlagCapsLock)?KBD_SYNC_CAPS_LOCK:0;
            [self.client enqueue:^(rdpInput *i) { freerdp_input_send_synchronize_event(i,flags); }]; return;
        }
        default: return;
    }
    BOOL down=(e.modifierFlags & mask) && ![self.held containsObject:@(e.keyCode)];
    [self sendKey:e.keyCode down:down];
    if(down) [self.held addObject:@(e.keyCode)]; else [self.held removeObject:@(e.keyCode)];
}
- (void)releaseKeys {
    for(NSNumber *key in self.held) [self sendKey:key.unsignedIntValue down:NO];
    [self.held removeAllObjects];
    UINT16 buttons=self.heldButtons;self.heldButtons=0;
    if(buttons) {
        UINT16 x=(UINT16)self.lastPointer.x,y=(UINT16)self.lastPointer.y;
        [self.client enqueue:^(rdpInput *i) {
            for(UINT16 button=PTR_FLAGS_BUTTON1;button<=PTR_FLAGS_BUTTON3;button<<=1)
                if(buttons&button)freerdp_input_send_mouse_event(i,button,x,y);
        }];
    }
}
- (BOOL)resignFirstResponder { [self releaseKeys]; return YES; }
- (void)pointer:(NSEvent *)e flags:(UINT16)flags {
    if(NSIsEmptyRect([self sourceRect]))return;
    NSPoint p=[self remotePoint:[self convertPoint:e.locationInWindow fromView:nil]];
    UINT16 x=(UINT16)p.x,y=(UINT16)p.y;
    self.lastPointer=p;
    UINT16 buttons=flags&(PTR_FLAGS_BUTTON1|PTR_FLAGS_BUTTON2|PTR_FLAGS_BUTTON3);
    if(flags&PTR_FLAGS_DOWN)self.heldButtons|=buttons;else self.heldButtons&=~buttons;
    void (^event)(rdpInput*)=^(rdpInput* i){freerdp_input_send_mouse_event(i,flags,x,y);};
    if(flags==PTR_FLAGS_MOVE)[self.client enqueueMovement:event];else [self.client enqueue:event];
}
- (NSPoint)remotePoint:(NSPoint)p {
    NSRect source=[self sourceRect],rect=[self imageRect];
    if(NSIsEmptyRect(source)||NSIsEmptyRect(rect))return NSZeroPoint;
    return NSMakePoint(source.origin.x+MAX(0,MIN(source.size.width-1,(p.x-rect.origin.x)*source.size.width/rect.size.width)),
        source.origin.y+MAX(0,MIN(source.size.height-1,(p.y-rect.origin.y)*source.size.height/rect.size.height)));
}
- (void)mouseDown:(NSEvent *)e { [self.window makeFirstResponder:self]; [self pointer:e flags:PTR_FLAGS_BUTTON1|PTR_FLAGS_DOWN]; }
- (void)mouseUp:(NSEvent *)e { [self pointer:e flags:PTR_FLAGS_BUTTON1]; }
- (void)rightMouseDown:(NSEvent *)e { [self pointer:e flags:PTR_FLAGS_BUTTON2|PTR_FLAGS_DOWN]; }
- (void)rightMouseUp:(NSEvent *)e { [self pointer:e flags:PTR_FLAGS_BUTTON2]; }
- (void)otherMouseDown:(NSEvent *)e { [self pointer:e flags:PTR_FLAGS_BUTTON3|PTR_FLAGS_DOWN]; }
- (void)otherMouseUp:(NSEvent *)e { [self pointer:e flags:PTR_FLAGS_BUTTON3]; }
- (void)mouseMoved:(NSEvent *)e { [self pointer:e flags:PTR_FLAGS_MOVE]; }
- (void)mouseDragged:(NSEvent *)e { [self mouseMoved:e]; }
- (void)rightMouseDragged:(NSEvent *)e { [self mouseMoved:e]; }
- (void)otherMouseDragged:(NSEvent *)e { [self mouseMoved:e]; }
- (void)scrollWheel:(NSEvent *)e {
    double deltas[]={e.scrollingDeltaY,e.scrollingDeltaX};
    for(int axis=0;axis<2;axis++) if(deltas[axis]!=0) {
        int amount=(int)MAX(-255,MIN(255,deltas[axis]*(e.hasPreciseScrollingDeltas?1:120)));
        UINT16 flags=(axis?PTR_FLAGS_HWHEEL:PTR_FLAGS_WHEEL)|(amount & 0x1ff);
        [self pointer:e flags:flags];
    }
}
- (void)insertText:(id)value replacementRange:(NSRange)range {
    NSString *text=[value isKindOfClass:NSAttributedString.class]?[value string]:value;
    for(NSUInteger n=0;n<text.length;n++) { UINT16 ch=[text characterAtIndex:n];
        [self.client enqueue:^(rdpInput *i) { freerdp_input_send_unicode_keyboard_event(i,0,ch); freerdp_input_send_unicode_keyboard_event(i,KBD_FLAGS_RELEASE,ch); }];
    }
    [self unmarkText];
}
- (void)doCommandBySelector:(SEL)selector {
    // An IME may clear its marked text before calling this callback. Do not
    // delete a remote committed character for that same composing Backspace.
    if(self.composingDeletion || self.hasMarkedText) return;
    unsigned key;
    if(selector==@selector(deleteBackward:)) key=0x33;
    else if(selector==@selector(deleteForward:)) key=0x75;
    else return;
    [self sendKey:key down:YES]; [self sendKey:key down:NO];
}
- (void)setMarkedText:(id)value selectedRange:(NSRange)selected replacementRange:(NSRange)replacement {
    self.marked=[value isKindOfClass:NSAttributedString.class]?[value mutableCopy]:[[NSMutableAttributedString alloc] initWithString:value];
}
- (void)unmarkText { self.marked=[NSMutableAttributedString new]; }
- (BOOL)hasMarkedText { return self.marked.length>0; }
- (NSRange)markedRange { return self.hasMarkedText?NSMakeRange(0,self.marked.length):NSMakeRange(NSNotFound,0); }
- (NSRange)selectedRange { return NSMakeRange(self.marked.length,0); }
- (NSArray *)validAttributesForMarkedText { return @[]; }
- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actual { return nil; }
- (NSUInteger)characterIndexForPoint:(NSPoint)point { return 0; }
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actual {
    return [self.window convertRectToScreen:[self convertRect:NSMakeRect(20,20,1,20) toView:nil]];
}
@end
