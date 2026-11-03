/* WinHomeDesk macOS ScreenCaptureKit shadow backend. Apache-2.0. */
#import <Foundation/Foundation.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <ApplicationServices/ApplicationServices.h>
#include "mac_input_sources.h" // Carbon declarations precede WinPR's CreateEvent macro.
#include <freerdp/server/shadow.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/region.h>
#include <winpr/input.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/sysinfo.h>
#include <winpr/collections.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include "mac_helpers.h"
#include "mac_video.h"
#import "mac_audio.h"
#import "mac_cursor.h"

/* The capture callback only replaces one retained pixel buffer. Encoding never
 * blocks ScreenCaptureKit's queue and old frames cannot accumulate. */
@interface WHDCapture : NSObject <SCStreamOutput, SCStreamDelegate>
@property(nonatomic, strong) SCStream *stream;
@property(nonatomic, strong) SCContentFilter *filter;
@property(nonatomic, strong) SCStreamConfiguration *config;
@property(nonatomic, strong) dispatch_queue_t queue;
@property(nonatomic) CVPixelBufferRef latest;
@property(nonatomic) HANDLE frameReady;
@property(nonatomic) uint64_t capturedAt;
@property(nonatomic) BOOL failed, loggedFrame, screenshot, inFlight, cancelled;
@property(nonatomic) uint64_t sequence, consumed, replaced;
@property(nonatomic) CFAbsoluteTime requestedAt;
@end
@implementation WHDCapture
- (void)store:(CMSampleBufferRef)sample {
    CVPixelBufferRef buffer=sample?CMSampleBufferGetImageBuffer(sample):NULL;
    if (!buffer) return;
    @synchronized(self) {
        if (_cancelled) return;
        if (!_loggedFrame) {
            fprintf(stderr,"WinHomeDesk CAPTURE first_frame=%zux%zu\n",CVPixelBufferGetWidth(buffer),CVPixelBufferGetHeight(buffer));
            _loggedFrame=YES;
        }
        if (_sequence != _consumed) _replaced++;
        if (_latest) CVPixelBufferRelease(_latest);
        _latest=CVPixelBufferRetain(buffer); _sequence++;
        double age=CMTimeGetSeconds(CMTimeSubtract(CMClockGetTime(CMClockGetHostTimeClock()),CMSampleBufferGetPresentationTimeStamp(sample)));
        uint64_t now=GetTickCount64();
        _capturedAt=now-(uint64_t)((isfinite(age)&&age>=0&&age<10)?age*1000:0);
        SetEvent(_frameReady);
    }
}
- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error {
    fprintf(stderr, "WinHomeDesk CAPTURE stopped: %s\n", error.localizedDescription.UTF8String);
    @synchronized(self) { if (!_screenshot && !_cancelled) _failed=YES; }
}
- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sample ofType:(SCStreamOutputType)type {
    if (type != SCStreamOutputTypeScreen || !CMSampleBufferIsValid(sample)) return;
    NSArray *attachments=(__bridge NSArray *)CMSampleBufferGetSampleAttachmentsArray(sample, false);
    if (!attachments.count || [attachments[0][SCStreamFrameInfoStatus] integerValue] != SCFrameStatusComplete) return;
    @synchronized(self) { if (!_screenshot) [self store:sample]; }
}
- (void)requestScreenshot {
    @synchronized(self) {
        if (_inFlight || _cancelled) return;
        _inFlight=YES; _requestedAt=CFAbsoluteTimeGetCurrent();
    }
    [SCScreenshotManager captureSampleBufferWithFilter:self.filter configuration:self.config completionHandler:^(CMSampleBufferRef sample, NSError *error) {
        @synchronized(self) {
            self.inFlight=NO;
            if (self.cancelled) return;
            if (error || !sample) {
                self.failed=YES;
                fprintf(stderr,"WinHomeDesk CAPTURE screenshot failed: %s\n",error.localizedDescription.UTF8String ?: "empty sample");
            } else [self store:sample];
        }
    }];
}
- (void)dealloc { if (_latest) CVPixelBufferRelease(_latest); }
@end

typedef struct {
    rdpShadowSubsystem base;
    HANDLE thread, stop, frameReady;
    void *capture, *audio, *cursor;
    CGRect bounds;
    CGDirectDisplayID display;
    CRITICAL_SECTION inputLock;
    BOOL keys[128], buttons[5];
    CGEventFlags modifiers;
    rdpShadowClient *controller;
    UINT16 highSurrogate[2];
    uint8_t imeHeld;
    CFAbsoluteTime lastClick[5];
    CGPoint clickPoint[5];
    unsigned clickCount[5];
    BOOL synthetic, audioEnabled;
    unsigned fps;
    size_t nativeWidth, nativeHeight;
} WHDShadow;

static BOOL unlocked(void) {
    CFDictionaryRef session = CGSessionCopyCurrentDictionary();
    if (!session) return FALSE;
    BOOL locked = CFDictionaryContainsKey(session, CFSTR("CGSSessionScreenIsLocked")) &&
        CFDictionaryGetValue(session, CFSTR("CGSSessionScreenIsLocked")) == kCFBooleanTrue;
    BOOL console = CFDictionaryGetValue(session, kCGSessionOnConsoleKey) == kCFBooleanTrue;
    CFRelease(session);
    return console && !locked;
}
static BOOL input_allowed(WHDShadow *s) { return !s->synthetic && unlocked() && AXIsProcessTrusted(); }
static int authenticate(rdpShadowSubsystem *base, rdpShadowClient *client,
                        const char *user, const char *domain, const char *password) {
    BYTE salt[16], expected[32], actual[32] = {0};
    BOOL ok = user && !strcasecmp(user, "surface") && password && *password &&
        (((WHDShadow *)base)->synthetic || unlocked()) &&
        whd_hex(getenv("SURFACE_DESK_SALT"), salt, sizeof(salt)) &&
        whd_hex(getenv("SURFACE_DESK_HASH"), expected, sizeof(expected));
    if (ok) ok = PKCS5_PBKDF2_HMAC_SHA1(password, (int)strlen(password), salt, sizeof(salt),
                                      200000, sizeof(actual), actual) == 1 &&
                 CRYPTO_memcmp(actual, expected, sizeof(actual)) == 0;
    OPENSSL_cleanse(actual, sizeof(actual)); OPENSSL_cleanse(expected, sizeof(expected));
    if (!ok) Sleep(1200);
    fprintf(stderr, "WinHomeDesk AUTH %s\n", ok ? "accepted" : "rejected");
    return ok ? 1 : -1;
}
static void post_key(WHDShadow *s, CGKeyCode key, BOOL down) {
    CGEventRef e = CGEventCreateKeyboardEvent(NULL, key, down);
    if (e) { CGEventSetFlags(e, s->modifiers); CGEventPost(kCGHIDEventTap, e); CFRelease(e); }
}
static CGEventFlags modifiers(WHDShadow *s) {
    return ((s->keys[56] || s->keys[60]) ? kCGEventFlagMaskShift : 0) |
           ((s->keys[59] || s->keys[62]) ? kCGEventFlagMaskControl : 0) |
           ((s->keys[58] || s->keys[61]) ? kCGEventFlagMaskAlternate : 0) |
           ((s->keys[55] || s->keys[54]) ? kCGEventFlagMaskCommand : 0) |
           (s->modifiers & kCGEventFlagMaskAlphaShift);
}
static BOOL keyboard(rdpShadowSubsystem *base, rdpShadowClient *client, UINT16 flags, UINT8 scan) {
    WHDShadow *s = (WHDShadow *)base;
    if (s->synthetic) fprintf(stderr,"WinHomeDesk TEST_INPUT keyboard\n");
    else if (!input_allowed(s)) return TRUE;
    UINT32 layout = freerdp_settings_get_uint32(client->context.settings, FreeRDP_KeyboardLayout);
    EnterCriticalSection(&s->inputLock);
    enum whd_ime_action ime = whd_hangul_event(&s->imeHeld, scan,
        (flags & KBD_FLAGS_EXTENDED) != 0, (flags & KBD_FLAGS_RELEASE) != 0,
        (layout & 0xffff) == 0x0412);
    LeaveCriticalSection(&s->inputLock);
    if (ime != WHD_IME_NONE) {
        if (ime == WHD_IME_TOGGLE) {
            /* Complete the switch before processing subsequent letter keys.
             * The Apple CLI pumps the main run loop while the server runs. */
            dispatch_sync(dispatch_get_main_queue(), ^{
                EnterCriticalSection(&s->inputLock);
                if (s->controller == client && (s->synthetic || input_allowed(s))) {
                    if (s->synthetic) fprintf(stderr,"WinHomeDesk TEST_INPUT ime_toggle\n");
                    else {
                        OSStatus result = whd_toggle_korean_source();
                        fprintf(stderr, "WinHomeDesk INPUT_IME key=hangul scan=0x%02X status=%d%s\n",
                            scan, (int)result, result == noErr ? "" : " (enable Korean and Latin input sources in macOS)");
                    }
                }
                LeaveCriticalSection(&s->inputLock);
            });
        }
        return TRUE;
    }
    if (s->synthetic) return TRUE; // Test pattern never changes input sources or posts OS input.
    DWORD vk = GetVirtualKeyCodeFromVirtualScanCode(scan | ((flags & KBD_FLAGS_EXTENDED) ? KBDEXT : 0), 4);
    if (!vk) return TRUE;
    if (flags & KBD_FLAGS_EXTENDED) vk |= KBDEXT;
    DWORD key = GetKeycodeFromVirtualKeyCode(vk, WINPR_KEYCODE_TYPE_APPLE);
    /* Zero is the valid macOS A key, so validate via reverse lookup. */
    if (key >= 128 || GetVirtualKeyCodeFromKeycode(key, WINPR_KEYCODE_TYPE_APPLE) != vk) return TRUE;
    EnterCriticalSection(&s->inputLock);
    BOOL down = !(flags & KBD_FLAGS_RELEASE); // RDP down need not carry KBD_FLAGS_DOWN
    if (key == 57 && down && !s->keys[key]) s->modifiers ^= kCGEventFlagMaskAlphaShift;
    s->keys[key] = down; s->modifiers = modifiers(s);
    post_key(s, (CGKeyCode)key, down);
    LeaveCriticalSection(&s->inputLock);
    return TRUE;
}
static BOOL unicode_key(rdpShadowSubsystem *base, rdpShadowClient *client, UINT16 flags, UINT16 code) {
    WHDShadow *s = (WHDShadow *)base;
    if (s->synthetic) { fprintf(stderr,"WinHomeDesk TEST_INPUT unicode\n"); return TRUE; }
    if (!input_allowed(s)) return TRUE;
    unsigned index = (flags & KBD_FLAGS_RELEASE) ? 1 : 0;
    EnterCriticalSection(&s->inputLock);
    if (code >= 0xD800 && code <= 0xDBFF) {
        s->highSurrogate[index] = code; LeaveCriticalSection(&s->inputLock); return TRUE;
    }
    UniChar text[2] = { code, 0 }; unsigned length = 1;
    if (code >= 0xDC00 && code <= 0xDFFF) {
        if (!s->highSurrogate[index]) { LeaveCriticalSection(&s->inputLock); return TRUE; }
        text[0] = s->highSurrogate[index]; text[1] = code; length = 2;
    }
    s->highSurrogate[index] = 0;
    CGEventRef e = CGEventCreateKeyboardEvent(NULL, 0, index == 0);
    if (e) { CGEventKeyboardSetUnicodeString(e, length, text); CGEventPost(kCGHIDEventTap, e); CFRelease(e); }
    LeaveCriticalSection(&s->inputLock);
    return TRUE;
}
static CGPoint point(WHDShadow *s, UINT16 x, UINT16 y) {
    rdpShadowSurface *surface = s->base.server->surface;
    return CGPointMake(whd_coordinate(x, surface->width, s->bounds.origin.x, s->bounds.size.width),
                       whd_coordinate(y, surface->height, s->bounds.origin.y, s->bounds.size.height));
}
static void post_mouse(WHDShadow *s, CGEventType type, CGPoint p, CGMouseButton button) {
    CGEventRef e = CGEventCreateMouseEvent(NULL, type, p, button);
    if (e) {
        if (type == kCGEventLeftMouseDown || type == kCGEventRightMouseDown || type == kCGEventOtherMouseDown) {
            CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
            BOOL near = fabs(p.x-s->clickPoint[button].x)<4 && fabs(p.y-s->clickPoint[button].y)<4;
            s->clickCount[button] = near && now-s->lastClick[button]<0.5 ? s->clickCount[button]+1 : 1;
            s->lastClick[button] = now; s->clickPoint[button] = p;
        }
        CGEventSetIntegerValueField(e, kCGMouseEventClickState, s->clickCount[button]);
        CGEventSetFlags(e, s->modifiers); CGEventPost(kCGHIDEventTap, e); CFRelease(e);
    }
}
static BOOL mouse(rdpShadowSubsystem *base, rdpShadowClient *client, UINT16 flags, UINT16 x, UINT16 y) {
    WHDShadow *s = (WHDShadow *)base;
    if (s->synthetic) { fprintf(stderr,"WinHomeDesk TEST_INPUT mouse x=%u y=%u flags=%u\n",x,y,flags); return TRUE; }
    if (!input_allowed(s)) return TRUE;
    EnterCriticalSection(&s->inputLock);
    if (flags & (PTR_FLAGS_WHEEL | PTR_FLAGS_HWHEEL)) {
        int delta = whd_wheel(flags);
        CGEventRef e = CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitPixel, 2,
            (flags & PTR_FLAGS_WHEEL) ? delta : 0, (flags & PTR_FLAGS_HWHEEL) ? delta : 0);
        if (e) { CGEventPost(kCGHIDEventTap, e); CFRelease(e); }
    } else {
        CGPoint p = point(s, x, y);
        if (flags & PTR_FLAGS_MOVE) {
            unsigned b = s->buttons[0] ? 0 : s->buttons[1] ? 1 : s->buttons[2] ? 2 : 5;
            CGEventType type = b == 0 ? kCGEventLeftMouseDragged : b == 1 ? kCGEventRightMouseDragged :
                b == 2 ? kCGEventOtherMouseDragged : kCGEventMouseMoved;
            post_mouse(s, type, p, b == 5 ? 0 : b);
        }
        const UINT16 masks[] = { PTR_FLAGS_BUTTON1, PTR_FLAGS_BUTTON2, PTR_FLAGS_BUTTON3 };
        for (unsigned b = 0; b < 3; b++) if (flags & masks[b]) {
            BOOL down = (flags & PTR_FLAGS_DOWN) != 0; s->buttons[b] = down;
            CGEventType type = b == 0 ? (down ? kCGEventLeftMouseDown : kCGEventLeftMouseUp) :
                b == 1 ? (down ? kCGEventRightMouseDown : kCGEventRightMouseUp) :
                         (down ? kCGEventOtherMouseDown : kCGEventOtherMouseUp);
            post_mouse(s, type, p, b);
        }
    }
    LeaveCriticalSection(&s->inputLock);
    return TRUE;
}
static BOOL extended_mouse(rdpShadowSubsystem *base, rdpShadowClient *client, UINT16 flags, UINT16 x, UINT16 y) {
    WHDShadow *s = (WHDShadow *)base;
    if (!input_allowed(s)) return TRUE;
    EnterCriticalSection(&s->inputLock);
    for (int b = 3; b < 5; b++) if (flags & (b == 3 ? PTR_XFLAGS_BUTTON1 : PTR_XFLAGS_BUTTON2)) {
        BOOL down = (flags & PTR_XFLAGS_DOWN) != 0; s->buttons[b] = down;
        post_mouse(s, down ? kCGEventOtherMouseDown : kCGEventOtherMouseUp, point(s, x, y), b);
    }
    LeaveCriticalSection(&s->inputLock);
    return TRUE;
}
static BOOL synchronize(rdpShadowSubsystem *base, rdpShadowClient *client, UINT32 flags) {
    WHDShadow *s = (WHDShadow *)base;
    EnterCriticalSection(&s->inputLock);
    if (flags & KBD_SYNC_CAPS_LOCK) s->modifiers |= kCGEventFlagMaskAlphaShift;
    else s->modifiers &= ~kCGEventFlagMaskAlphaShift;
    LeaveCriticalSection(&s->inputLock);
    return TRUE;
}
static void release_inputs(rdpShadowSubsystem *base, rdpShadowClient *client) {
    WHDShadow *s = (WHDShadow *)base;
    EnterCriticalSection(&s->inputLock);
    if (client && client != s->controller) { LeaveCriticalSection(&s->inputLock); return; }
    s->controller = NULL;
    memset(s->highSurrogate, 0, sizeof(s->highSurrogate));
    s->imeHeld = 0;
    s->modifiers = 0;
    for (int k = 0; k < 128; k++) if (s->keys[k]) { post_key(s, k, FALSE); s->keys[k] = FALSE; }
    CGEventRef current = CGEventCreate(NULL);
    CGPoint p = current ? CGEventGetLocation(current) : CGPointZero;
    if (current) CFRelease(current);
    for (int b = 0; b < 5; b++) if (s->buttons[b]) {
        post_mouse(s, b == 0 ? kCGEventLeftMouseUp : b == 1 ? kCGEventRightMouseUp : kCGEventOtherMouseUp, p, b);
        s->buttons[b] = FALSE;
    }
    LeaveCriticalSection(&s->inputLock);
}
static BOOL client_connect(rdpShadowSubsystem *base, rdpShadowClient *client) {
    WHDShadow *s = (WHDShadow *)base;
    EnterCriticalSection(&s->inputLock);
    BOOL allowed = !s->controller || s->controller == client;
    if (allowed) s->controller = client;
    LeaveCriticalSection(&s->inputLock);
    if (allowed) fprintf(stderr, "WinHomeDesk INPUT keyboard_layout=0x%08X type=%u subtype=%u\n",
        freerdp_settings_get_uint32(client->context.settings, FreeRDP_KeyboardLayout),
        freerdp_settings_get_uint32(client->context.settings, FreeRDP_KeyboardType),
        freerdp_settings_get_uint32(client->context.settings, FreeRDP_KeyboardSubType));
    return allowed; // One controller avoids interleaved held-key state.
}
static UINT32 monitors(MONITOR_DEF *m, UINT32 count) {
    if (!m || !count) return 0;
    CGDisplayModeRef mode=CGDisplayCopyDisplayMode(CGMainDisplayID());
    unsigned nativeWidth=mode?(unsigned)CGDisplayModeGetPixelWidth(mode):0;
    unsigned nativeHeight=mode?(unsigned)CGDisplayModeGetPixelHeight(mode):0;
    if(mode) CGDisplayModeRelease(mode);
    if (getenv("WINHOMEDESK_TEST_PATTERN")) { nativeWidth=800; nativeHeight=600; }
    unsigned w, h;
    if (!whd_video_size(nativeWidth,nativeHeight,whd_video_edge(getenv("SURFACE_DESK_MAX_EDGE")),&w,&h)) return 0;
    m[0] = (MONITOR_DEF){ .left=0, .top=0, .right=w-1, .bottom=h-1, .flags=1 };
    return 1;
}
static DWORD WINAPI deliver(LPVOID arg) {
    @autoreleasepool {
        WHDShadow *s=arg; rdpShadowSurface *surface=s->base.server->surface;
        WHDCapture *capture=(__bridge WHDCapture *)s->capture;
        unsigned tick=0; uint64_t sequence=0, updates=0, idle=0;
        double copyMs=0, waitMs=0, area=0, ageMs=0, ageMax=0;
        uint64_t ageSamples=0;
        CFAbsoluteTime reportAt=CFAbsoluteTimeGetCurrent(), next=reportAt;
        DWORD delay=0;
        for (;;) {
            BOOL screenshotMode;@synchronized(capture){screenshotMode=capture.screenshot;}
            if(s->synthetic || screenshotMode) {if(WaitForSingleObject(s->stop,delay)!=WAIT_TIMEOUT)break;}
            else {
                HANDLE events[]={s->stop,s->frameReady};
                DWORD result=WaitForMultipleObjects(2,events,FALSE,250);
                if(result==WAIT_OBJECT_0 || result==WAIT_FAILED)break;
                ResetEvent(s->frameReady);
            }
            @autoreleasepool {
                CFAbsoluteTime began=CFAbsoluteTimeGetCurrent();
                if (!s->synthetic) {
                    CGDisplayModeRef mode=CGDisplayCopyDisplayMode(s->display);
                    BOOL changed=!mode || CGDisplayModeGetPixelWidth(mode)!=s->nativeWidth || CGDisplayModeGetPixelHeight(mode)!=s->nativeHeight;
                    if(mode) CGDisplayModeRelease(mode);
                    if (!unlocked() || changed || s->display!=CGMainDisplayID() || !CGRectEqualToRect(s->bounds,CGDisplayBounds(s->display))) {
                        fprintf(stderr,"WinHomeDesk: desktop locked, switched, or display changed; stopping.\n");
                        SetEvent(s->base.server->StopEvent); break;
                    }
                }
                CVPixelBufferRef buffer=NULL; BOOL failed=NO, screenshot=NO;
                @synchronized(capture) {
                    failed=capture.failed; screenshot=capture.screenshot;
                    if (capture.inFlight && began-capture.requestedAt>5) failed=YES;
                }
                if(failed && !screenshot) {
                    /* Same automatic fallback policy as DXGI -> GDI on Windows. */
                    @synchronized(capture) { capture.screenshot=YES; capture.failed=NO; }
                    [capture.stream stopCaptureWithCompletionHandler:^(NSError *e) {}];
                    fprintf(stderr,"WinHomeDesk CAPTURE backend=SCScreenshotManager reason=stream-runtime-failure\n");
                    screenshot=YES; failed=NO;
                }
                if(failed) { SetEvent(s->base.server->StopEvent); break; }
                if(screenshot) [capture requestScreenshot];
                @synchronized(capture) {
                    if(capture.latest && capture.sequence!=sequence) {
                        sequence=capture.sequence; capture.consumed=sequence;
                        buffer=CVPixelBufferRetain(capture.latest);
                        double age=GetTickCount64()-capture.capturedAt;ageMs+=age;ageMax=fmax(ageMax,age);ageSamples++;
                    }
                }
                EnterCriticalSection(&surface->lock);
                BOOL copied=FALSE; unsigned dirty[4]={0,0,surface->width,surface->height};
                if(s->synthetic) {
                    for(int y=0;y<surface->height;y++) for(int x=0;x<surface->width;x++) {
                        BYTE *p=surface->data+y*surface->scanline+x*4;
                        p[0]=x&255; p[1]=y&255; p[2]=tick&255; p[3]=255;
                    }
                    tick++; copied=TRUE;
                } else if(buffer && CVPixelBufferGetWidth(buffer)==surface->width && CVPixelBufferGetHeight(buffer)==surface->height &&
                          CVPixelBufferLockBaseAddress(buffer,kCVPixelBufferLock_ReadOnly)==kCVReturnSuccess) {
                    copied=whd_video_copy(surface->data,surface->scanline,CVPixelBufferGetBaseAddress(buffer),
                        CVPixelBufferGetBytesPerRow(buffer),surface->width,surface->height,dirty);
                    CVPixelBufferUnlockBaseAddress(buffer,kCVPixelBufferLock_ReadOnly);
                }
                if(buffer) CVPixelBufferRelease(buffer);
                if(copied) {
                    RECTANGLE_16 rect={dirty[0],dirty[1],dirty[2],dirty[3]};
                    region16_union_rect(&surface->invalidRegion,&surface->invalidRegion,&rect);
                    updates++; area+=(double)(dirty[2]-dirty[0])*(dirty[3]-dirty[1])/surface->width/surface->height;
                } else idle++;
                LeaveCriticalSection(&surface->lock);
                if(s->synthetic && s->audioEnabled)[(__bridge WHDAudio*)s->audio syntheticTick];
                CFAbsoluteTime copiedAt=CFAbsoluteTimeGetCurrent(); copyMs+=(copiedAt-began)*1000;
                /* Publish even when idle so newly connected clients can refresh the
                 * retained surface. An empty invalid region sends no duplicate frame. */
                if(sequence || s->synthetic) shadow_subsystem_frame_update(&s->base);
                waitMs+=(CFAbsoluteTimeGetCurrent()-copiedAt)*1000;
                EnterCriticalSection(&surface->lock); region16_clear(&surface->invalidRegion); LeaveCriticalSection(&surface->lock);
                wMessage message;
                while(MessageQueue_Peek(s->base.MsgPipe->In,&message,TRUE)) if(message.Free) message.Free(&message);
                CFAbsoluteTime now=CFAbsoluteTimeGetCurrent();
                if(now-reportAt>=2) {
                    uint64_t replaced; @synchronized(capture) { replaced=capture.replaced; capture.replaced=0; }
                    fprintf(stderr,"WinHomeDesk PERF_CAPTURE fps=%.1f target=%u copy_ms=%.2f wait_ms=%.2f area_pct=%.1f idle=%llu replaced=%llu age_ms=%.2f age_max_ms=%.2f pending_limit=1\n",
                        updates/(now-reportAt),s->fps,copyMs/fmax(1,updates+idle),waitMs/fmax(1,updates+idle),100*area/fmax(1,updates),
                        (unsigned long long)idle,(unsigned long long)replaced,ageMs/fmax(1,ageSamples),ageMax);
                    reportAt=now; updates=idle=ageSamples=0; area=copyMs=waitMs=ageMs=ageMax=0;
                }
                next+=1.0/s->fps;
                if(next<now) next=now;
                delay=(DWORD)ceil(fmax(0,next-now)*1000);
            }
        }
    }
    return 0;
}
static int initialize(rdpShadowSubsystem *base) {
    WHDShadow *s = (WHDShadow *)base;
    s->synthetic = getenv("WINHOMEDESK_TEST_PATTERN") != NULL;
    if (s->synthetic && (!base->server->ipcSocket ||
        strcmp(base->server->ipcSocket, "bind-address,127.0.0.1") != 0)) {
        fprintf(stderr, "WinHomeDesk: test-pattern mode requires /bind-address:127.0.0.1\n");
        return -1;
    }
    if (base->selectedMonitor != 0) return -1;
    if (!s->synthetic && (!CGPreflightScreenCaptureAccess() || !AXIsProcessTrusted() || !unlocked())) {
        fprintf(stderr, "WinHomeDesk: grant Screen Recording and Accessibility to the server, then restart it in an unlocked session.\n"); return -1;
    }
    base->numMonitors = monitors(base->monitors, 16);
    if (!base->numMonitors) return -1;
    base->virtualScreen = base->monitors[0];
    // Two-monitor protocol fixture only in the existing loopback/no-OS-input mode.
    const char* testMonitors=getenv("WINHOMEDESK_TEST_MONITORS");
    if(s->synthetic && testMonitors) {
        base->surfaceAllMonitors=TRUE;base->numMonitors=2;
        if(!strcmp(testMonitors,"vertical")) {
            base->monitors[0]=(MONITOR_DEF){.left=0,.top=0,.right=799,.bottom=299,.flags=1};
            base->monitors[1]=(MONITOR_DEF){.left=0,.top=-300,.right=799,.bottom=-1,.flags=0};
        } else {
            base->monitors[0]=(MONITOR_DEF){.left=0,.top=0,.right=399,.bottom=599,.flags=1};
            base->monitors[1]=(MONITOR_DEF){.left=-400,.top=0,.right=-1,.bottom=599,.flags=0};
        }
    }
    s->display = CGMainDisplayID(); s->bounds = CGDisplayBounds(s->display);
    s->fps=whd_video_fps(getenv("SURFACE_DESK_FPS"));
    const char* audio=getenv("SURFACE_DESK_AUDIO");s->audioEnabled=audio && !strcmp(audio,"1");
    if(s->audioEnabled)s->audio=(__bridge_retained void*)[[WHDAudio alloc] initWithSubsystem:base];
    CGDisplayModeRef mode=CGDisplayCopyDisplayMode(s->display);
    s->nativeWidth=mode?CGDisplayModeGetPixelWidth(mode):0;
    s->nativeHeight=mode?CGDisplayModeGetPixelHeight(mode):0;
    if(mode) CGDisplayModeRelease(mode);
    s->stop = CreateEvent(NULL, TRUE, FALSE, NULL);
    s->frameReady = CreateEvent(NULL, TRUE, FALSE, NULL);
    base->captureFrameRate=s->fps;
    base->server->ShowMouseCursor=TRUE;
    return s->stop && s->frameReady ? 1 : -1;
}
static int start(rdpShadowSubsystem *base) {
    @autoreleasepool {
        WHDShadow *s = (WHDShadow *)base;
        WHDCapture *capture = [WHDCapture new];
        capture.frameReady=s->frameReady;
        s->capture = (__bridge_retained void *)capture;
        if (!s->synthetic) {
            dispatch_semaphore_t ready = dispatch_semaphore_create(0);
            __block SCShareableContent *content = nil;
            [SCShareableContent getShareableContentExcludingDesktopWindows:NO onScreenWindowsOnly:YES completionHandler:^(SCShareableContent *c, NSError *error) { content = c; dispatch_semaphore_signal(ready); }];
            if (dispatch_semaphore_wait(ready, dispatch_time(DISPATCH_TIME_NOW, 15*NSEC_PER_SEC)) || !content) return -1;
            SCDisplay *display = nil;
            for (SCDisplay *d in content.displays) if (d.displayID == s->display) display = d;
            if (!display) return -1;
            SCContentFilter *filter = [[SCContentFilter alloc] initWithDisplay:display excludingWindows:@[]];
            SCStreamConfiguration *config = [SCStreamConfiguration new];
            config.width = base->server->surface->width; config.height = base->server->surface->height;
            config.pixelFormat = kCVPixelFormatType_32BGRA; config.queueDepth = 3;
            config.minimumFrameInterval = CMTimeMake(1,s->fps); config.showsCursor = NO;
            config.capturesAudio = NO;
            capture.filter=filter; capture.config=config;
            if(s->audioEnabled && ![(__bridge WHDAudio*)s->audio startWithFilter:filter]) {
                fprintf(stderr,"WinHomeDesk AUDIO state=start-failed; video remains available\n");
            }
            const char *mode=getenv("SURFACE_DESK_CAPTURE");
            capture.screenshot=mode && (!strcmp(mode,"screenshot") || !strcmp(mode,"gdi"));
            if(!capture.screenshot) {
                capture.queue=dispatch_queue_create("WinHomeDesk.capture",DISPATCH_QUEUE_SERIAL);
                capture.stream=[[SCStream alloc] initWithFilter:filter configuration:config delegate:capture];
                NSError *error=nil;
                __block BOOL started=NO;
                if([capture.stream addStreamOutput:capture type:SCStreamOutputTypeScreen sampleHandlerQueue:capture.queue error:&error]) {
                    [capture.stream startCaptureWithCompletionHandler:^(NSError *e) { started=!e; dispatch_semaphore_signal(ready); }];
                    if(dispatch_semaphore_wait(ready,dispatch_time(DISPATCH_TIME_NOW,15*NSEC_PER_SEC))) started=NO;
                }
                if(!started) {
                    @synchronized(capture) { capture.screenshot=YES; capture.failed=NO; }
                    [capture.stream stopCaptureWithCompletionHandler:^(NSError *e) {}];
                    fprintf(stderr,"WinHomeDesk CAPTURE fallback=stream-start-failed\n");
                }
            }

        }
        if(!s->synthetic) {
            WHDCursor* cursor=[[WHDCursor alloc] initWithSubsystem:base bounds:s->bounds];
            s->cursor=(__bridge_retained void*)cursor;[cursor start];
        }
        s->thread = CreateThread(NULL, 0, deliver, s, 0, NULL);
        fprintf(stderr,"WinHomeDesk CAPTURE backend=%s source=%zux%zu stream=%dx%d fps=%u\n",
            s->synthetic?"test-pattern-no-input":capture.screenshot?"SCScreenshotManager":"ScreenCaptureKit",
            s->nativeWidth,s->nativeHeight,base->server->surface->width,base->server->surface->height,s->fps);
        return s->thread ? 1 : -1;
    }
}
static int stop(rdpShadowSubsystem *base) {
    WHDShadow *s = (WHDShadow *)base;
    if (s->stop) SetEvent(s->stop);
    if(s->cursor){WHDCursor* cursor=(__bridge_transfer WHDCursor*)s->cursor;s->cursor=NULL;[cursor stop];}
    if (s->thread) shadow_client_boardcast_quit(base->server, 0);
    if (s->thread) { WaitForSingleObject(s->thread, INFINITE); CloseHandle(s->thread); s->thread=NULL; }
    if(s->audio){WHDAudio* audio=(__bridge_transfer WHDAudio*)s->audio;s->audio=NULL;[audio stop];}
    if (s->capture) {
        WHDCapture *capture = (__bridge_transfer WHDCapture *)s->capture; s->capture=NULL;
        @synchronized(capture) { capture.cancelled=YES; }
        if (capture.stream) {
            dispatch_semaphore_t done = dispatch_semaphore_create(0);
            [capture.stream stopCaptureWithCompletionHandler:^(NSError *e) { dispatch_semaphore_signal(done); }];
            dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 5*NSEC_PER_SEC));
            [capture.stream removeStreamOutput:capture type:SCStreamOutputTypeScreen error:NULL];
        }
    }
    release_inputs(base, NULL);
    return 1;
}
static int uninitialize(rdpShadowSubsystem *base) {
    WHDShadow *s = (WHDShadow *)base; stop(base);
    if(s->frameReady){CloseHandle(s->frameReady);s->frameReady=NULL;}
    if(s->stop) { CloseHandle(s->stop); s->stop=NULL; } return 1;
}
static void destroy(rdpShadowSubsystem *base) {
    if(!base) return; uninitialize(base); DeleteCriticalSection(&((WHDShadow *)base)->inputLock); free(base);
}
static rdpShadowSubsystem *create(void) {
    WHDShadow *s = calloc(1, sizeof(*s)); if (!s) return NULL;
    InitializeCriticalSection(&s->inputLock);
    s->base.Authenticate=authenticate; s->base.KeyboardEvent=keyboard; s->base.UnicodeKeyboardEvent=unicode_key;
    s->base.MouseEvent=mouse; s->base.ExtendedMouseEvent=extended_mouse;
    s->base.SynchronizeEvent=synchronize; s->base.ClientDisconnect=release_inputs;
    s->base.ClientConnect=client_connect;
    return &s->base;
}
FREERDP_API const char *ShadowSubsystemName(void) { return "Mac"; }
FREERDP_API int ShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS *ep) {
    ep->New=create; ep->Free=destroy; ep->Init=initialize; ep->Uninit=uninitialize;
    ep->Start=start; ep->Stop=stop; ep->EnumMonitors=monitors; return 1;
}
