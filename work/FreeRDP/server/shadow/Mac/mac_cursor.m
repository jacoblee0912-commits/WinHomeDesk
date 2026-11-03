#import "mac_cursor.h"
#import "mac_cursor_pixels.h"
#include <freerdp/codec/color.h>
#include <winpr/collections.h>
#include <winpr/sysinfo.h>
@interface WHDCursor ()
@property(nonatomic) rdpShadowSubsystem* base;
@property(nonatomic) CGRect bounds;
@property(nonatomic,strong) dispatch_source_t timer;
@property(nonatomic,strong) NSData* previous;
@property(nonatomic) NSPoint hotspot;
@property(nonatomic) uint64_t lastSent;
@property(atomic) BOOL cancelled;
@end
static void free_cursor(UINT32 id,SHADOW_MSG_OUT* common) {
    SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE* p=(void*)common;
    free(p->andMaskData);free(p->xorMaskData);free(p);
}
@implementation WHDCursor
- (instancetype)initWithSubsystem:(rdpShadowSubsystem*)base bounds:(CGRect)bounds {
    if((self=[super init])){_base=base;_bounds=bounds;}return self;
}
- (void)poll {
    if(self.cancelled)return;
    rdpShadowServer* server=self.base->server;
    ArrayList_Lock(server->clients);
    BOOL ready=NO;
    for(size_t i=0;i<ArrayList_Count(server->clients);i++) {
        rdpShadowClient* c=ArrayList_GetItem(server->clients,i);
        if(c->activated && c->mayView && !c->inLobby && MessageQueue_Size(c->MsgQueue)<8){ready=YES;break;}
    }
    ArrayList_Unlock(server->clients);
    if(!ready)return;
    NSCursor* cursor=NSCursor.currentSystemCursor ?: NSCursor.arrowCursor;
    NSImage* image=cursor.image;
    double scale=server->surface->width/self.bounds.size.width;
    unsigned w=(unsigned)fmax(1,fmin(96,ceil(image.size.width*scale)));
    unsigned h=(unsigned)fmax(1,fmin(96,ceil(image.size.height*scale)));
    NSPoint hot=NSMakePoint(fmin(w-1,fmax(0,round(cursor.hotSpot.x*scale))),fmin(h-1,fmax(0,round(cursor.hotSpot.y*scale))));
    NSData* data=whd_cursor_pixels(image,w,h);if(!data)return;
    uint64_t now=GetTickCount64();
    if([data isEqualToData:self.previous] && NSEqualPoints(hot,self.hotspot) && now-self.lastSent<1000)return;
    self.previous=data;self.hotspot=hot;self.lastSent=now;
    SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE* packet=calloc(1,sizeof(*packet));if(!packet)return;
    packet->common.Free=free_cursor;packet->width=w;packet->height=h;packet->xHot=hot.x;packet->yHot=hot.y;
    if(shadow_subsystem_pointer_convert_alpha_pointer_data_to_format(data.bytes,PIXEL_FORMAT_BGRA32,TRUE,w,h,packet)<0){free(packet);return;}
    shadow_client_boardcast_msg(server,NULL,SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE_ID,(SHADOW_MSG_OUT*)packet,NULL);
}
- (void)start {
    self.timer=dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,0,0,dispatch_get_main_queue());
    __weak WHDCursor* weak=self;
    dispatch_source_set_event_handler(self.timer, ^{[weak poll];});
    dispatch_source_set_timer(self.timer,DISPATCH_TIME_NOW,NSEC_PER_SEC/30,NSEC_PER_MSEC);
    dispatch_resume(self.timer);
    fprintf(stderr,"WinHomeDesk CURSOR mode=client-local shape_channel=rdp refresh_hz=30\n");
}
- (void)stop {
    self.cancelled=YES;
    if(self.timer){dispatch_source_cancel(self.timer);self.timer=nil;}
    if(!NSThread.isMainThread)dispatch_sync(dispatch_get_main_queue(), ^{});
}
@end
