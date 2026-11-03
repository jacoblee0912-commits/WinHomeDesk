#define main whd_unused_app_main
#import "../App.m"
#undef main
#include <assert.h>
static void pump(double seconds) {NSDate* until=[NSDate dateWithTimeIntervalSinceNow:seconds];while(until.timeIntervalSinceNow>0)[NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.01]];}
@interface MonitorTestApp : WHDApp
@end
@implementation MonitorTestApp
- (WHDClient*)createClient {WHDClient* c=[super createClient];c.loopbackTest=YES;return c;}
@end
static void geometry(void) {
    WHDClient* c=[WHDClient new];c.desktopSize=NSMakeSize(200,100);
    MONITOR_DEF m[]={{0,0,99,99,1},{-100,0,-1,99,0}};
    assert([c setRemoteMonitors:m count:2]);
    WHDRemoteView* view=[[WHDRemoteView alloc] initWithFrame:NSMakeRect(0,0,400,300)];view.client=c;view.monitorIndex=0;
    assert(NSEqualRects(view.sourceRect,NSMakeRect(100,0,100,100)));
    assert(NSEqualRects(view.imageRect,NSMakeRect(50,0,300,300)));
    assert(NSEqualPoints([view remotePoint:NSMakePoint(200,150)],NSMakePoint(150,50)));
    assert(NSEqualPoints([view remotePoint:NSMakePoint(-10,999)],NSMakePoint(100,99)));
    view.monitorIndex=1;assert(NSEqualPoints([view remotePoint:NSMakePoint(200,150)],NSMakePoint(50,50)));
    unsigned char pixels[200*100*4];
    for(int y=0;y<100;y++)for(int x=0;x<200;x++){unsigned char* p=pixels+(y*200+x)*4;p[0]=y>=50?255:0;p[1]=x>=100?255:0;p[2]=x<100?255:0;p[3]=255;}
    NSData* bytes=[NSData dataWithBytes:pixels length:sizeof(pixels)];CGDataProviderRef provider=CGDataProviderCreateWithCFData((__bridge CFDataRef)bytes);
    CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();CGImageRef cg=CGImageCreate(200,100,8,32,800,space,kCGBitmapByteOrder32Little|kCGImageAlphaNoneSkipFirst,provider,NULL,false,kCGRenderingIntentDefault);
    c.image=[[NSImage alloc] initWithCGImage:cg size:c.desktopSize];CGImageRelease(cg);CGColorSpaceRelease(space);CGDataProviderRelease(provider);
    NSImage* target=[[NSImage alloc] initWithSize:view.bounds.size];[target lockFocus];[view drawRect:view.bounds];[target unlockFocus];
    NSImage* crop=[view valueForKey:@"cropImage"];NSBitmapImageRep* rep=[[NSBitmapImageRep alloc] initWithCGImage:[crop CGImageForProposedRect:NULL context:nil hints:nil]];
    assert(rep.pixelsWide==100 && rep.pixelsHigh==100);
    NSColor* top=[[rep colorAtX:50 y:25] colorUsingColorSpace:NSColorSpace.deviceRGBColorSpace];
    assert(top.redComponent>.9 && top.blueComponent<.1 && top.greenComponent<.1);
    MONITOR_DEF vertical[]={{0,0,199,49,1},{0,-50,199,-1,0}};assert([c setRemoteMonitors:vertical count:2]);view.monitorIndex=0;
    assert(NSEqualRects(view.sourceRect,NSMakeRect(0,50,200,50)));
    [target lockFocus];[view drawRect:view.bounds];[target unlockFocus];crop=[view valueForKey:@"cropImage"];
    rep=[[NSBitmapImageRep alloc] initWithCGImage:[crop CGImageForProposedRect:NULL context:nil hints:nil]];
    top=[[rep colorAtX:50 y:10] colorUsingColorSpace:NSColorSpace.deviceRGBColorSpace];assert(top.redComponent>.9 && top.blueComponent>.9);
    assert(![c setRemoteMonitors:m count:17]);m[0].left=INT32_MIN;m[0].right=INT32_MAX;assert(![c setRemoteMonitors:m count:2]);
    view.monitorIndex=5;assert(NSIsEmptyRect(view.sourceRect));
    puts("PASS monitor view: negative origins, letterboxing, input offsets/clamps, horizontal/vertical crop pixels, malformed layout rejection");
}
int main(int argc,char** argv){@autoreleasepool{
    [NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];geometry();
    if(argc==1)return 0;assert(argc==4);
    char secret[1024]={0};assert(fgets(secret,sizeof(secret),stdin));secret[strcspn(secret,"\r\n")]=0;
    MonitorTestApp* app=[MonitorTestApp new];NSApp.delegate=app;app.sessions=[NSMutableArray new];app.log=[NSTextView new];
    app.host=[NSTextField new];app.host.stringValue=@"127.0.0.1";app.port=[NSTextField new];app.port.stringValue=@(argv[1]);
    app.user=[NSTextField new];app.user.stringValue=@"surface";app.domain=[NSTextField new];app.password=[NSSecureTextField new];app.password.stringValue=@(secret);memset(secret,0,sizeof(secret));
    app.clientGraphics=[NSPopUpButton new];[app.clientGraphics addItemsWithTitles:whdGraphicsTitles()];[app.clientGraphics selectItemAtIndex:atoi(argv[2])];
    [app connect:nil];WHDRemoteSession* first=app.sessions[0];
    for(int i=0;i<800 && (first.client.frames<3 || first.client.monitors.count!=2);i++)pump(.01);
    assert(first.client.connected && first.client.monitors.count==2 && first.client.image);[app updateMonitors:first];
    [app connect:nil];WHDRemoteSession* second=app.sessions[1];
    assert(first.client==second.client && first.view.monitorIndex==0 && second.view.monitorIndex==1);
    assert(first.monitorChoice.numberOfItems==3 && second.monitorChoice.selectedItem.tag==1);
    BOOL vertical=!strcmp(argv[3],"vertical");
    NSRect a=vertical?NSMakeRect(0,300,800,300):NSMakeRect(400,0,400,600);
    NSRect b=vertical?NSMakeRect(0,0,800,300):NSMakeRect(0,0,400,600);
    assert(NSEqualRects(first.view.sourceRect,a) && NSEqualRects(second.view.sourceRect,b));
    for(WHDRemoteSession* s in app.sessions) {
        NSRect rect=s.view.imageRect;NSPoint local=NSMakePoint(NSMidX(rect),NSMidY(rect));NSPoint remote=[s.view remotePoint:local];
        printf("MONITOR_INPUT index=%ld x=%.0f y=%.0f\n",(long)s.view.monitorIndex,remote.x,remote.y);
        NSEvent* e=[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:[s.view convertPoint:local toView:nil] modifierFlags:0 timestamp:0 windowNumber:s.window.windowNumber context:nil eventNumber:1 clickCount:1 pressure:1];
        [s.view mouseDown:e];[s.view releaseKeys]; // release on focus loss must stay on this monitor
    }
    pump(.2);[first.window close];NSUInteger frames=second.client.frames;pump(.3);
    assert(second.client.connected && !second.client.stop && second.client.frames>frames);
    [second.window close];pump(.5);assert(second.client.finished);
    puts("PASS authenticated two-monitor RDP: server layout, per-monitor windows, distinct source rectangles, wire input and shared lifetime");
}return 0;}
