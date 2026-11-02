/* Surface Desk prototype, 2026. Apache-2.0.
 * Uses the FreeRDP shadow-server interface. No TermService or RDPWrap changes.
 */
#include <windows.h>
#include <wchar.h>
#include <freerdp/server/shadow.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/region.h>
#include <winpr/synch.h>
#include <winpr/collections.h>
#include <stdio.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include "surface_diff.h"
#include "surface_dxgi.h"
#include "surface_audio.h"
#include "surface_latest.h"
#include "surface_pacer.h"
#include "surface_keyboard.h"
#include "../surface_perf.h"

typedef struct {
    rdpShadowSubsystem base;
    HANDLE thread;
    HANDLE audioThread;
    HANDLE deliveryThread;
    HANDLE captureTimer;
    SurfaceLatest* latest;
    HDC screen, memory;
    HBITMAP bitmap;
    HGDIOBJ previous;
    BYTE* pixels;
    int width, height, sourceWidth, sourceHeight;
    double frameInterval;
    SurfaceDxgi* dxgi;
    BOOL haveFrame, strictDxgi;
} SurfaceSubsystem;

static BOOL desktop_available(void) {
    HDESK d = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    WCHAR name[128] = {0}; DWORD needed = 0;
    if (!d) return FALSE;
    BOOL ok = GetUserObjectInformationW(d, UOI_NAME, name, sizeof(name), &needed);
    CloseDesktop(d);
    return ok && _wcsicmp(name, L"Default") == 0;
}

static BOOL decode_hex(const char* text, BYTE* data, size_t n) {
    if (!text || strlen(text)!=n*2) return FALSE;
    for(size_t i=0;i<n;i++) {
        unsigned int v=0;
        if(sscanf(text+i*2,"%2x",&v)!=1) return FALSE;
        data[i]=(BYTE)v;
    }
    return TRUE;
}
static int auth_failure(const char* reason) {
    fprintf(stderr,"SurfaceDesk authentication: rejected (%s)\n",reason);
    fflush(stderr);
    Sleep(1200);
    return -1;
}
static int authenticate(rdpShadowSubsystem* base, rdpShadowClient* client,
                        const char* user, const char* domain, const char* password) {
    BYTE salt[16], expected[32], actual[32];
    if (!user || !user[0]) return auth_failure("username missing");
    if (_stricmp(user,"surface")) return auth_failure("username must be surface");
    if (!password || !password[0]) return auth_failure("password not supplied by client");
    if (!desktop_available()) return auth_failure("interactive desktop unavailable");
    if(!decode_hex(getenv("SURFACE_DESK_SALT"),salt,16) ||
       !decode_hex(getenv("SURFACE_DESK_HASH"),expected,32)) return auth_failure("server credential configuration invalid");
    BOOL ok=PKCS5_PBKDF2_HMAC_SHA1(password,(int)strlen(password),salt,16,200000,32,actual)==1;
    ok=ok && CRYPTO_memcmp(actual,expected,32)==0;
    SecureZeroMemory(actual,sizeof(actual));SecureZeroMemory(expected,sizeof(expected));
    if(!ok) return auth_failure("password mismatch or derivation failed");
    fprintf(stderr,"SurfaceDesk authentication: accepted\n");
    fflush(stderr);
    return 1;
}

static BOOL record_input(rdpShadowClient* c,BOOL success) {
    if(!success)return FALSE;
    c->surfaceInputCount++;
    if(c->surfaceEncodeBusy) {
        c->surfaceInputDuringEncode++;
        if(c->surfaceInputDuringEncode==1 || GetTickCount64()-c->surfaceInputReport>=2000) {
            fprintf(stderr,"SurfaceDesk INPUT processed=%llu during_encode=%llu\n",(unsigned long long)c->surfaceInputCount,(unsigned long long)c->surfaceInputDuringEncode);
            fflush(stderr);c->surfaceInputReport=GetTickCount64();
        }
    }
    return TRUE;
}
static BOOL keyboard(rdpShadowSubsystem* b,rdpShadowClient* c,UINT16 flags,UINT8 code) {
    if (!desktop_available()) return FALSE;
    INPUT events[2];
    DWORD targetThread=GetWindowThreadProcessId(GetForegroundWindow(),NULL);
    HKL layout=GetKeyboardLayout(targetThread);
    const char* setting=getenv("WINHOME_DESK_RALT_HANGUL");
    BOOL rightAltHangul=(!setting || strcmp(setting,"0")!=0);
    UINT count=surface_keyboard_events_ex(&c->surfaceImeHeld,flags,code,layout,rightAltHangul,events);
    if(code==0x38 && (flags & KBD_FLAGS_EXTENDED)) {
        fprintf(stderr,"SurfaceDesk INPUT_ALT key=right-alt flags=0x%04X mode=%s target_layout=0x%08lX client_layout=0x%08lX action=%s\n",
            flags,rightAltHangul?"hangul":"layout",(unsigned long)(ULONG_PTR)layout,
            (unsigned long)freerdp_settings_get_uint32(c->context.settings,FreeRDP_KeyboardLayout),
            !count?"repeat-or-release-suppressed":count==2?"hangul":"scancode");fflush(stderr);
    }
    if(!count)return TRUE;
    UINT sent=SendInput(count,events,sizeof(INPUT));
    if(count==2) {
        fprintf(stderr,"SurfaceDesk INPUT_IME key=%s scan=0x%02X release=%u injected=%u expected=%u\n",
            events[0].u.ki.wVk==VK_HANGUL?"hangul":"hanja",code,
            (flags & KBD_FLAGS_RELEASE)?1:0,sent,count);fflush(stderr);
        if(sent!=count)c->surfaceImeHeld=0;
    }
    return record_input(c,sent==count);
}
static BOOL unicode_key(rdpShadowSubsystem* b,rdpShadowClient* c,UINT16 flags,UINT16 code) {
    if (!desktop_available()) return FALSE;
    INPUT e={0}; e.type=INPUT_KEYBOARD; e.u.ki.wScan=code; e.u.ki.dwFlags=KEYEVENTF_UNICODE;
    if (flags & KBD_FLAGS_RELEASE) e.u.ki.dwFlags |= KEYEVENTF_KEYUP;
    return record_input(c,SendInput(1,&e,sizeof(e)) == 1);
}
static BOOL mouse(rdpShadowSubsystem* base,rdpShadowClient* c,UINT16 flags,UINT16 x,UINT16 y) {
    SurfaceSubsystem* s=(SurfaceSubsystem*)base;
    if (!desktop_available()) return FALSE;
    INPUT e={0}; e.type=INPUT_MOUSE;
    if (flags & (PTR_FLAGS_WHEEL|PTR_FLAGS_HWHEEL)) {
        e.u.mi.dwFlags=(flags&PTR_FLAGS_WHEEL)?MOUSEEVENTF_WHEEL:MOUSEEVENTF_HWHEEL;
        int amount=flags&0x1ff; if (flags&PTR_FLAGS_WHEEL_NEGATIVE) amount-=0x200;
        e.u.mi.mouseData=(DWORD)amount;
    } else {
        e.u.mi.dx=MulDiv(x,65535,s->width-1); e.u.mi.dy=MulDiv(y,65535,s->height-1);
        e.u.mi.dwFlags=MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_MOVE;
        if (flags&PTR_FLAGS_BUTTON1) e.u.mi.dwFlags|=(flags&PTR_FLAGS_DOWN)?MOUSEEVENTF_LEFTDOWN:MOUSEEVENTF_LEFTUP;
        if (flags&PTR_FLAGS_BUTTON2) e.u.mi.dwFlags|=(flags&PTR_FLAGS_DOWN)?MOUSEEVENTF_RIGHTDOWN:MOUSEEVENTF_RIGHTUP;
        if (flags&PTR_FLAGS_BUTTON3) e.u.mi.dwFlags|=(flags&PTR_FLAGS_DOWN)?MOUSEEVENTF_MIDDLEDOWN:MOUSEEVENTF_MIDDLEUP;
    }
    return record_input(c,SendInput(1,&e,sizeof(e))==1);
}
static UINT32 monitors(MONITOR_DEF* m,UINT32 count) {
    if (!count) return 0;
    int w=GetSystemMetrics(SM_CXSCREEN),h=GetSystemMetrics(SM_CYSCREEN);
    const char* setting=getenv("SURFACE_DESK_MAX_EDGE");
    int edge=setting?atoi(setting):1920;
    int longest=w>h?w:h;
    if(edge>=640 && edge<longest) {
        w=(int)((int64_t)w*edge/longest)&~3;
        h=(int)((int64_t)h*edge/longest)&~3;
    }
    m[0].left=m[0].top=0; m[0].right=w-1;
    m[0].bottom=h-1; m[0].flags=1; return 1;
}
static int capture_frame(SurfaceSubsystem* s) {
    if(s->dxgi) {
        HRESULT hr=S_OK;
        int result=surface_dxgi_capture(s->dxgi,s->pixels,(size_t)s->width*4,&hr);
        if(result>=0)return result;
        fprintf(stderr,"SurfaceDesk DXGI failed: hr=0x%08lx; %s\n",(unsigned long)hr,
                s->strictDxgi?"strict mode stops":"falling back to GDI");
        surface_dxgi_free(s->dxgi);s->dxgi=NULL;
        if(s->strictDxgi){SetEvent(s->base.server->StopEvent);return -1;}
        fprintf(stderr,"SurfaceDesk CAPTURE backend=gdi reason=dxgi_runtime_failure\n");fflush(stderr);
    }
    BOOL copied;
    if(s->width==s->sourceWidth && s->height==s->sourceHeight)
        copied=BitBlt(s->memory,0,0,s->width,s->height,s->screen,0,0,SRCCOPY|CAPTUREBLT);
    else copied=StretchBlt(s->memory,0,0,s->width,s->height,s->screen,0,0,
                          s->sourceWidth,s->sourceHeight,SRCCOPY|CAPTUREBLT);
    GdiFlush();return copied?1:-1;
}
static DWORD WINAPI capture(LPVOID arg) {
    SurfaceSubsystem* s=arg; rdpShadowServer* server=s->base.server;
    rdpShadowSurface* surface=server->surface;
    const char* mode=getenv("SURFACE_DESK_CAPTURE");
    s->strictDxgi=mode && strcmp(mode,"dxgi")==0;
    if(!mode || strcmp(mode,"gdi")!=0) {
        HRESULT hr=S_OK;
        s->dxgi=surface_dxgi_create(s->sourceWidth,s->sourceHeight,s->width,s->height,&hr);
        if(!s->dxgi) {
            fprintf(stderr,"SurfaceDesk DXGI unavailable: hr=0x%08lx; %s\n",(unsigned long)hr,
                    s->strictDxgi?"strict mode stops":"falling back to GDI");
            if(s->strictDxgi){SetEvent(server->StopEvent);return 0;}
        }
    }
    fprintf(stderr,"SurfaceDesk CAPTURE backend=%s source=%dx%d stream=%dx%d resize=%s\n",
        s->dxgi?"dxgi":"gdi",s->sourceWidth,s->sourceHeight,s->width,s->height,
        (s->width==s->sourceWidth && s->height==s->sourceHeight)?"none":
        (s->dxgi?"GPU-linear":"GDI-halftone"));fflush(stderr);
    double delay=0;
    double nextCapture=surface_perf_now();
    double windowStart=surface_perf_now(), captureMs=0, pollMs=0, copyMs=0, waitMs=0, workMax=0;
    UINT64 attempts=0, frames=0, changes=0, late=0, errors=0, idle=0;
    while (surface_pacer_wait(s->captureTimer,server->StopEvent,delay)) {
        delay=(DWORD)(s->frameInterval+0.999);
        if (!desktop_available() || GetSystemMetrics(SM_CXSCREEN)!=s->sourceWidth ||
            GetSystemMetrics(SM_CYSCREEN)!=s->sourceHeight) {
            fprintf(stderr,"SurfaceDesk stopped: desktop locked, secured, or resized.\n");
            SetEvent(server->StopEvent); break;
        }
        const double workStart=surface_perf_now();
        attempts++;
        int copied=capture_frame(s);
        const double capturedAt=surface_perf_now();
        pollMs+=capturedAt-workStart;
        if(copied<0){errors++;goto capture_report;}
        if(copied==0){idle++;goto capture_report;}
        captureMs+=capturedAt-workStart;frames++;s->haveFrame=TRUE;
        if(surface_latest_put(s->latest,s->pixels,capturedAt))changes++;
        copyMs+=surface_perf_now()-capturedAt;
capture_report:
        {
            const double end=surface_perf_now(), workMs=end-workStart;
            if(workMs>workMax)workMax=workMs;
        }
        nextCapture+=s->frameInterval;
        double now=surface_perf_now();
        if(nextCapture<=now){late++;nextCapture=now+1;} /* Drop missed deadlines, never queue old frames. */
        delay=nextCapture-now;
        const double measuredAt=surface_perf_now(), seconds=(measuredAt-windowStart)/1000.0;
        if(seconds>=2.0) {
            const double n=frames?(double)frames:1.0;
            fprintf(stderr,"SurfaceDesk PERF_CAPTURE window_s=%.3f target=%u frames=%llu "
                "fps=%.2f changed_fps=%.2f capture_ms=%.3f copy_ms=%.3f wait_ms=%.3f "
                "work_max_ms=%.3f late=%llu errors=%llu backend=%s idle_polls=%llu poll_ms=%.3f pipeline=latest replaced=%llu\n",
                seconds,s->base.captureFrameRate,(unsigned long long)frames,frames/seconds,
                0.0,captureMs/n,copyMs/n,waitMs/(attempts?(double)attempts:1.0),
                workMax,(unsigned long long)late,(unsigned long long)errors,s->dxgi?"dxgi":"gdi",
                (unsigned long long)idle,pollMs/(attempts?(double)attempts:1.0),(unsigned long long)changes);
            fflush(stderr);
            windowStart=measuredAt;captureMs=pollMs=copyMs=waitMs=workMax=0;
            attempts=frames=changes=late=errors=idle=0;
        }
    }
    return 0;
}
/* Only this thread updates the shared FreeRDP surface. It stays immutable until
 * all subscribers consume it. Capture can overwrite the separate pending slot. */
static DWORD WINAPI deliver(LPVOID arg) {
    SurfaceSubsystem* s=arg;rdpShadowServer* server=s->base.server;
    rdpShadowSurface* surface=server->surface;
    HANDLE events[]={server->StopEvent,s->latest->ready};BOOL haveFrame=FALSE;
    double report=surface_perf_now(),copyMs=0,waitMs=0,ageMs=0,ageMax=0;
    UINT64 taken=0,changed=0,sequence=0,skipped=0,previous=0,published=0;
    while(WaitForMultipleObjects(2,events,FALSE,100)!=WAIT_OBJECT_0) {
        if(WaitForSingleObject(server->StopEvent,0)==WAIT_OBJECT_0)break;
        BYTE* pixels=NULL;double capturedAt=0;
        if(surface_latest_take(s->latest,&pixels,&sequence,&capturedAt)) {
            double begin=surface_perf_now(),age=begin-capturedAt;
            ageMs+=age;if(age>ageMax)ageMax=age;
            taken++;skipped+=sequence-previous-1;previous=sequence;
            EnterCriticalSection(&surface->lock);
            SurfaceDirtyRect dirty;
            if(surface_copy_changed(surface->data,surface->scanline,pixels,
                                    (size_t)s->width*4,s->width,s->height,&dirty)) {
                RECTANGLE_16 r={(UINT16)dirty.left,(UINT16)dirty.top,(UINT16)dirty.right,(UINT16)dirty.bottom};
                if(!region16_union_rect(&surface->invalidRegion,&surface->invalidRegion,&r)) {
                    LeaveCriticalSection(&surface->lock);SetEvent(server->StopEvent);return 0;
                }
                changed++;
            }
            LeaveCriticalSection(&surface->lock);
            copyMs+=surface_perf_now()-begin;haveFrame=TRUE;
        }
        if(haveFrame) {
            double begin=surface_perf_now();
            /* The multiclient event has its own subscriber/lifetime lock. Do not
             * hold server->clients across encoding; audio needs that list too. */
            shadow_subsystem_frame_update(&s->base);
            published++;
            waitMs+=surface_perf_now()-begin;
            EnterCriticalSection(&surface->lock);
            region16_clear(&surface->invalidRegion);
            LeaveCriticalSection(&surface->lock);
        }
        double now=surface_perf_now(),seconds=(now-report)/1000.0;
        if(seconds>=2.0) {
            double n=taken?(double)taken:1.0;
            fprintf(stderr,"SurfaceDesk PERF_PIPELINE window_s=%.3f taken=%llu changed=%llu replaced=%llu "
                "snapshot_ms=%.3f age_ms=%.3f age_max_ms=%.3f delivery_wait_ms=%.3f pending_limit=1\n",
                seconds,(unsigned long long)taken,(unsigned long long)changed,(unsigned long long)skipped,
                copyMs/n,ageMs/n,ageMax,waitMs/(published?(double)published:1.0));fflush(stderr);
            report=now;copyMs=waitMs=ageMs=ageMax=0;taken=changed=skipped=published=0;
        }
    }
    return 0;
}
static int init(rdpShadowSubsystem* base) {
    SurfaceSubsystem* s=(SurfaceSubsystem*)base;
    SetProcessDPIAware();
    if (!desktop_available()) return -1;
    base->numMonitors=monitors(base->monitors,16); base->virtualScreen=base->monitors[0];
    const char* fpsSetting=getenv("SURFACE_DESK_FPS");
    int fps=fpsSetting?atoi(fpsSetting):20;
    if(fps!=20 && fps!=30 && fps!=45)fps=20;
    base->captureFrameRate=(UINT32)fps;s->frameInterval=1000.0/fps;
    surface_audio_formats(base);
    s->sourceWidth=GetSystemMetrics(SM_CXSCREEN);s->sourceHeight=GetSystemMetrics(SM_CYSCREEN);
    s->width=base->monitors[0].right+1; s->height=base->monitors[0].bottom+1;
    if (s->width<2||s->height<2||s->width>8192||s->height>8192) return -1;
    BOOL highResolution=FALSE;s->captureTimer=surface_pacer_new(&highResolution);
    if(!s->captureTimer)return -1;
    fprintf(stderr,"SurfaceDesk PACER clock=qpc timer=%s\n",highResolution?"high-resolution":"standard");
    s->latest=surface_latest_new((size_t)s->width*s->height*4);
    if(!s->latest)return -1;
    s->screen=GetDC(NULL); s->memory=CreateCompatibleDC(s->screen);
    BITMAPINFO bi={0}; bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth=s->width; bi.bmiHeader.biHeight=-s->height;
    bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=32; bi.bmiHeader.biCompression=BI_RGB;
    s->bitmap=CreateDIBSection(s->screen,&bi,DIB_RGB_COLORS,(void**)&s->pixels,NULL,0);
    if (!s->memory||!s->bitmap||!s->pixels) return -1;
    s->previous=SelectObject(s->memory,s->bitmap);
    SetStretchBltMode(s->memory,HALFTONE);SetBrushOrgEx(s->memory,0,0,NULL);
    fprintf(stderr,"SurfaceDesk screen: source=%dx%d stream=%dx%d, target=%d fps, dirty rectangles, app-password authentication\n",
            s->sourceWidth,s->sourceHeight,s->width,s->height,fps);
    return 1;
}
static int start(rdpShadowSubsystem* base) {
    SurfaceSubsystem* s=(SurfaceSubsystem*)base;
    s->deliveryThread=CreateThread(NULL,0,deliver,s,0,NULL);
    if(!s->deliveryThread)return -1;
    fprintf(stderr,"SurfaceDesk PIPELINE mode=latest pending_limit=1 capture_independent=1\n");
    s->thread=CreateThread(NULL,0,capture,s,0,NULL);
    if(!s->thread)return -1;
    if(surface_audio_enabled()) {
        s->audioThread=CreateThread(NULL,0,surface_audio_thread,base,0,NULL);
        if(!s->audioThread)fprintf(stderr,"SurfaceDesk AUDIO state=error hr=thread-create\n");
    } else fprintf(stderr,"SurfaceDesk AUDIO state=disabled\n");
    fflush(stderr);return 1;
}
static int stop(rdpShadowSubsystem* base) {
    SurfaceSubsystem* s=(SurfaceSubsystem*)base;
    SetEvent(base->server->StopEvent);
    if(s->thread) { WaitForSingleObject(s->thread,INFINITE); CloseHandle(s->thread);s->thread=NULL; }
    if(s->deliveryThread) { WaitForSingleObject(s->deliveryThread,INFINITE); CloseHandle(s->deliveryThread);s->deliveryThread=NULL; }
    if(s->audioThread) { WaitForSingleObject(s->audioThread,INFINITE); CloseHandle(s->audioThread);s->audioThread=NULL; }
    return 1;
}
static int uninit(rdpShadowSubsystem* base) {
    SurfaceSubsystem* s=(SurfaceSubsystem*)base;
    surface_latest_free(s->latest);s->latest=NULL;
    if(s->captureTimer){CloseHandle(s->captureTimer);s->captureTimer=NULL;}
    surface_dxgi_free(s->dxgi);s->dxgi=NULL;
    if(s->memory&&s->previous) SelectObject(s->memory,s->previous);
    if(s->bitmap) DeleteObject(s->bitmap);
    if(s->memory) DeleteDC(s->memory);
    if(s->screen) ReleaseDC(NULL,s->screen);
    s->bitmap=NULL;s->memory=NULL;s->screen=NULL;s->pixels=NULL;return 1;
}
static void destroy(rdpShadowSubsystem* base) { if(base) { uninit(base);free(base); } }
static rdpShadowSubsystem* create(void) {
    SurfaceSubsystem* s=calloc(1,sizeof(*s)); if(!s) return NULL;
    s->base.Authenticate=authenticate; s->base.KeyboardEvent=keyboard;
    s->base.UnicodeKeyboardEvent=unicode_key; s->base.MouseEvent=mouse;
    return &s->base;
}
const char* ShadowSubsystemName(void) { return "SurfaceDesk-DXGI"; }
int ShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS* e) {
    e->New=create;e->Free=destroy;e->Init=init;e->Uninit=uninit;
    e->Start=start;e->Stop=stop;e->EnumMonitors=monitors;return 1;
}
