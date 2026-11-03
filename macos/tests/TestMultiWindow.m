/* Real AppKit windows + two single-controller synthetic loopback RDP servers. */
#define main whd_unused_app_main
#import "../App.m"
#undef main
#include <assert.h>
@interface MultiTestApp : WHDApp
@property NSString* connectionProblem;
@property NSUInteger createdClients;
@end
@implementation MultiTestApp
- (WHDClient*)createClient {self.createdClients++;WHDClient* c=[super createClient];c.loopbackTest=YES;return c;}
- (void)connectionError:(NSString*)message {self.connectionProblem=message;}
@end
static void pump(double seconds) {
    NSDate* until=[NSDate dateWithTimeIntervalSinceNow:seconds];
    while(until.timeIntervalSinceNow>0)[NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.01]];
}
int main(int argc,char** argv){@autoreleasepool{
    assert(argc==3);char secret[1024]={0};assert(fgets(secret,sizeof(secret),stdin));secret[strcspn(secret,"\r\n")]=0;
    [NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    MultiTestApp* app=[MultiTestApp new];NSApp.delegate=app;app.sessions=[NSMutableArray new];
    app.host=[NSTextField new];app.host.stringValue=@"127.0.0.1";
    app.port=[NSTextField new];app.user=[NSTextField new];app.user.stringValue=@"surface";
    app.domain=[NSTextField new];app.password=[NSSecureTextField new];
    app.log=[NSTextView new];
    app.clientGraphics=[NSPopUpButton new];[app.clientGraphics addItemsWithTitles:whdGraphicsTitles()];
    app.port.stringValue=@(argv[1]);app.password.stringValue=@(secret);[app connect:nil];
    assert(!app.password.stringValue.length);
    // Open another window while the first transport is still connecting, with
    // no retained password. Neither changed graphics nor whitespace opens TCP #2.
    app.host.stringValue=@" 127.0.0.1 ";[app.clientGraphics selectItemAtIndex:3];[app connect:nil];
    WHDRemoteSession* a=app.sessions[0];WHDRemoteSession* mirror=app.sessions[1];
    assert(a.client==mirror.client && a.view!=mirror.view && a.window!=mirror.window && a.client.graphicsMode==0);
    assert(app.createdClients==1);
    // Never reuse the authenticated connection for a different account/domain.
    app.user.stringValue=@"other";[app connect:nil];assert(app.connectionProblem && app.sessions.count==2);
    app.user.stringValue=@"surface";app.domain.stringValue=@"other";app.connectionProblem=nil;
    [app connect:nil];assert(app.connectionProblem && app.sessions.count==2);
    app.domain.stringValue=@"";app.port.stringValue=@(argv[2]);app.connectionProblem=nil;
    [app connect:nil];assert(app.connectionProblem && app.sessions.count==2 && app.createdClients==1);
    app.password.stringValue=@(secret);[app.clientGraphics selectItemAtIndex:0];[app connect:nil];
    WHDRemoteSession* b=app.sessions[2];assert(app.createdClients==2 && !app.password.stringValue.length);
    assert(a.client!=b.client && a.view!=b.view && a.window!=b.window);
    for(int i=0;i<500 && (a.client.frames<3 || b.client.frames<3);i++)pump(.01);
    assert(a.client.connected && b.client.connected && a.client.frames>=3 && b.client.frames>=3);
    assert(a.client.image && b.client.image && mirror.client.image);
    // Reproduce the old failure against the same one-controller limit used by
    // the Windows launcher: an unpooled second RDP login is rejected at transport.
    WHDClient* refused=[WHDClient new];refused.loopbackTest=YES;
    [refused connectHost:@"127.0.0.1" port:atoi(argv[1]) user:@"surface" domain:@"" password:@(secret)];
    for(int i=0;i<500 && !refused.finished;i++)pump(.01);
    assert(refused.finished && !refused.frames && [refused.status containsString:@"0x0002000d"]);
    assert(a.client.connected && b.client.connected);
    [a.view.held addObject:@56];[b.view.held addObject:@59];
    [app windowDidResignKey:[NSNotification notificationWithName:NSWindowDidResignKeyNotification object:a.window]];
    assert(a.view.held.count==0 && b.view.held.count==1);
    NSUInteger frames=b.client.frames,sharedFrames=mirror.client.frames;[a.window close];pump(.7);
    assert(a.closed && !mirror.client.stop && mirror.client.connected && mirror.client.frames>sharedFrames);
    assert(!b.client.stop && b.client.connected && b.client.frames>frames);
    // Opening a window after authentication also reuses the live connection.
    app.port.stringValue=@(argv[1]);[app connect:nil];WHDRemoteSession* third=app.sessions.lastObject;
    assert(third.client==mirror.client && app.createdClients==2);
    app.activeSession=mirror;[app disconnect:nil];pump(.2);assert(mirror.closed && third.client.connected && !third.client.stop);
    app.activeSession=third;[app disconnect:nil];pump(.7);assert(third.client.finished && b.client.connected);
    // The last close releases the one server slot; reconnect must use a new client.
    app.password.stringValue=@(secret);[app connect:nil];memset(secret,0,sizeof(secret));
    WHDRemoteSession* again=app.sessions.lastObject;assert(again.client!=a.client && app.createdClients==3);
    for(int i=0;i<500 && again.client.frames<3;i++)pump(.01);
    assert(again.client.connected && again.client.frames>=3);
    app.activeSession=b;[app disconnect:nil];pump(.5);assert(b.client.finished && again.client.connected);
    [again.window close];pump(.5);assert(again.client.finished);
    puts("PASS multiwindow: single-controller same-PC shared transport, pending/live reuse, independent PCs, credential guards, isolated close, last-window disconnect and reconnect");
}return 0;}
