/* Loopback-only integration probe. Not shipped as a remote-control client. */
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <freerdp/freerdp.h>
#include <freerdp/input.h>
#include <freerdp/settings.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/gfx.h>
#include <freerdp/client.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/channels.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/addin.h>
#include <winpr/synch.h>
static unsigned frames;
static unsigned gfxDecoded, lastCodec;
static pcRdpgfxSurfaceCommand decode_surface;
static UINT decoded_surface(RdpgfxClientContext* context,const RDPGFX_SURFACE_COMMAND* cmd) {
    UINT rc=decode_surface(context,cmd);
    if(rc==0){gfxDecoded++;lastCodec=cmd->codecId;}
    return rc;
}
static void channel_connected(void* context, const ChannelConnectedEventArgs* e) {
    rdpContext* c=(rdpContext*)context;
    if(strcmp(e->name,RDPGFX_DVC_CHANNEL_NAME)==0) {
        RdpgfxClientContext* gfx=(RdpgfxClientContext*)e->pInterface;
        gdi_graphics_pipeline_init(c->gdi,gfx);
        decode_surface=gfx->SurfaceCommand;gfx->SurfaceCommand=decoded_surface;
    }
}
static void channel_disconnected(void* context, const ChannelDisconnectedEventArgs* e) {
    rdpContext* c=(rdpContext*)context;
    if(strcmp(e->name,RDPGFX_DVC_CHANNEL_NAME)==0)
        gdi_graphics_pipeline_uninit(c->gdi,(RdpgfxClientContext*)e->pInterface);
}
static BOOL pre(freerdp* i) {
    return PubSub_SubscribeChannelConnected(i->context->pubSub,channel_connected)>=0 &&
           PubSub_SubscribeChannelDisconnected(i->context->pubSub,channel_disconnected)>=0;
}
static BOOL begin(rdpContext* c) { c->gdi->primary->hdc->hwnd->invalid->null=TRUE;return TRUE; }
static BOOL end(rdpContext* c) {
    if(!c->gdi->primary->hdc->hwnd->invalid->null) frames++;
    return TRUE;
}
static BOOL resize(rdpContext* c) {
    return gdi_resize(c->gdi,freerdp_settings_get_uint32(c->settings,FreeRDP_DesktopWidth),
                     freerdp_settings_get_uint32(c->settings,FreeRDP_DesktopHeight));
}
static BOOL post(freerdp* i) {
    if(!gdi_init(i,PIXEL_FORMAT_BGRX32))return FALSE;
    i->context->update->BeginPaint=begin;i->context->update->EndPaint=end;
    i->context->update->DesktopResize=resize;return TRUE;
}
int main(int argc,char**argv) {
    if(argc!=2 && argc!=3)return 2;
    WSADATA wsa; if(WSAStartup(MAKEWORD(2,2),&wsa)) return 2;
    freerdp* i=freerdp_new();if(!i)return 2;
    i->ContextSize=sizeof(rdpContext);i->PostConnect=post;i->PreConnect=pre;
    i->LoadChannels=freerdp_client_load_channels;
    freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry,0);
    if(!freerdp_context_new(i))return 2;
    rdpSettings* s=i->context->settings;
    BOOL audio=getenv("SURFACE_PROBE_AUDIO")!=NULL;
    freerdp_settings_set_bool(s,FreeRDP_AudioPlayback,audio);
    freerdp_settings_set_bool(s,FreeRDP_RemoteConsoleAudio,FALSE);
    if(audio) {
        const char* args[]={"rdpsnd","sys:fake"};
        if(!freerdp_client_add_static_channel(s,2,args))return 2;
    }
    freerdp_settings_set_bool(s,FreeRDP_AudioCapture,FALSE);
    freerdp_settings_set_bool(s,FreeRDP_RedirectClipboard,FALSE);
    freerdp_settings_set_bool(s,FreeRDP_DeviceRedirection,FALSE);
    freerdp_settings_set_bool(s,FreeRDP_RedirectPrinters,FALSE);
    freerdp_settings_set_string(s,FreeRDP_ServerHostname,"127.0.0.1");
    freerdp_settings_set_uint32(s,FreeRDP_ServerPort,13390);
    freerdp_settings_set_string(s,FreeRDP_Username,argc==3?argv[2]:"surface");
    freerdp_settings_set_string(s,FreeRDP_Password,argv[1]);
    freerdp_settings_set_bool(s,FreeRDP_NlaSecurity,FALSE);
    freerdp_settings_set_bool(s,FreeRDP_ExtSecurity,FALSE);
    freerdp_settings_set_bool(s,FreeRDP_RdpSecurity,FALSE);
    freerdp_settings_set_bool(s,FreeRDP_TlsSecurity,TRUE);
    freerdp_settings_set_bool(s,FreeRDP_IgnoreCertificate,TRUE); /* only 127.0.0.1 test */
    const char* gfx=getenv("SURFACE_PROBE_GFX");
    freerdp_settings_set_bool(s,FreeRDP_SupportGraphicsPipeline,gfx!=NULL);
    freerdp_settings_set_bool(s,FreeRDP_GfxH264,gfx && strcmp(gfx,"planar")!=0);
    freerdp_settings_set_bool(s,FreeRDP_GfxAVC444,gfx && strcmp(gfx,"444")==0);
    freerdp_settings_set_bool(s,FreeRDP_GfxAVC444v2,gfx && strcmp(gfx,"444")==0);
    if(getenv("SURFACE_PROBE_CAPS10"))
        freerdp_settings_set_uint32(s,FreeRDP_GfxCapsFilter,UINT32_MAX ^ (1u << 2));
    freerdp_settings_set_uint32(s,FreeRDP_DesktopWidth,1280);
    freerdp_settings_set_uint32(s,FreeRDP_DesktopHeight,720);
    freerdp_settings_set_uint32(s,FreeRDP_ColorDepth,32);
    freerdp_settings_set_uint32(s,FreeRDP_TcpConnectTimeout,5000);
    BOOL connected=freerdp_connect(i);
    if(connected) {
        BOOL imeSent=FALSE;
        const char* durationSetting=getenv("SURFACE_PROBE_DURATION_MS");
        const char* refreshSetting=getenv("SURFACE_PROBE_REFRESH_MS");
        DWORD duration=durationSetting?(DWORD)atoi(durationSetting):5000;
        DWORD refresh=refreshSetting?(DWORD)atoi(refreshSetting):0;
        if(duration<200 || duration>30000)duration=5000;
        if(refresh && refresh<100)refresh=100;
        BOOL suppressTest=getenv("SURFACE_PROBE_SUPPRESS")!=NULL;unsigned suppressState=0;ULONGLONG resumeAt=0;
        POINT original={0};BOOL mouseTest=getenv("SURFACE_PROBE_MOUSE")!=NULL;GetCursorPos(&original);
        ULONGLONG deadline=GetTickCount64()+duration, nextRefresh=GetTickCount64()+500;
        while(GetTickCount64()<deadline && !freerdp_shall_disconnect_context(i->context)) {
            HANDLE events[64];DWORD count=freerdp_get_event_handles(i->context,events,64);
            if(!count)break;
            WaitForMultipleObjects(count,events,FALSE,mouseTest?5:100);
            if(!freerdp_check_event_handles(i->context))break;
            if(!imeSent && frames>0 && getenv("WINHOME_PROBE_RALT")) {
                for(int tap=0;tap<2;tap++) {
                    if(!freerdp_input_send_keyboard_event(i->context->input,KBD_FLAGS_EXTENDED,0x38) ||
                       !freerdp_input_send_keyboard_event(i->context->input,KBD_FLAGS_EXTENDED|KBD_FLAGS_RELEASE,0x38))return 3;
                }
                imeSent=TRUE;
            }
            if(!imeSent && frames>0 && getenv("WINHOME_PROBE_IME")) {
                /* Wait for activation/first frame before sending two toggles. */
                if(!freerdp_input_send_keyboard_event(i->context->input,0,0x72) ||
                   !freerdp_input_send_keyboard_event(i->context->input,KBD_FLAGS_RELEASE,0x72) ||
                   !freerdp_input_send_keyboard_event(i->context->input,KBD_FLAGS_EXTENDED|KBD_FLAGS_RELEASE,0xF2))return 3;
                imeSent=TRUE;
            }
            if(suppressTest && gfxDecoded>0 && !suppressState) {
                if(!i->context->update->SuppressOutput(i->context,0,NULL))break;
                suppressState=1;resumeAt=GetTickCount64()+250;
            } else if(suppressState==1 && GetTickCount64()>=resumeAt) {
                RECTANGLE_16 area={0,0,(UINT16)(freerdp_settings_get_uint32(s,FreeRDP_DesktopWidth)-1),(UINT16)(freerdp_settings_get_uint32(s,FreeRDP_DesktopHeight)-1)};
                if(!i->context->update->SuppressOutput(i->context,1,&area))break;
                suppressState=2;
            }
            if(mouseTest) {
                UINT16 x=(UINT16)MulDiv(original.x,freerdp_settings_get_uint32(s,FreeRDP_DesktopWidth)-1,GetSystemMetrics(SM_CXSCREEN)-1);
                UINT16 y=(UINT16)MulDiv(original.y,freerdp_settings_get_uint32(s,FreeRDP_DesktopHeight)-1,GetSystemMetrics(SM_CYSCREEN)-1);
                if(!freerdp_input_send_mouse_event(i->context->input,PTR_FLAGS_MOVE,x,y))break;
            }
            if(refresh && GetTickCount64()>=nextRefresh) {
                RECTANGLE_16 area={0,0,
                    (UINT16)(freerdp_settings_get_uint32(s,FreeRDP_DesktopWidth)-1),
                    (UINT16)(freerdp_settings_get_uint32(s,FreeRDP_DesktopHeight)-1)};
                if(!i->context->update->RefreshRect(i->context,1,&area))break;
                nextRefresh=GetTickCount64()+refresh;
            }
            if(!durationSetting && frames>=2)break;
        }
        freerdp_disconnect(i);
        if(mouseTest)SetCursorPos(original.x,original.y);
    }
    printf("PROBE connected=%d frames=%u size=%ux%u\n",connected,frames,
           freerdp_settings_get_uint32(s,FreeRDP_DesktopWidth),
           freerdp_settings_get_uint32(s,FreeRDP_DesktopHeight));
    printf("PROBE gfx_decoded=%u codec=%u\n",gfxDecoded,lastCodec);
    if(i->context->gdi)gdi_free(i);
    freerdp_context_free(i);freerdp_free(i);
    return frames>0?0:1;
}
