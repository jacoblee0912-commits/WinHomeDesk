/* Exercise the real AppKit termination loop without windows, credentials or network. */
#define main whd_unused_app_main
#import "../App.m"
#undef main
@interface LifecycleApp : WHDApp
@end
@implementation LifecycleApp
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    puts("RUN: requesting quit while a worker is still active"); fflush(stdout);
    WHDRemoteSession* first=[WHDRemoteSession new];first.client=[WHDClient new];
    WHDRemoteSession* last=[WHDRemoteSession new];last.client=[WHDClient new];
    WHDRemoteSession* shared=[WHDRemoteSession new];shared.client=first.client;
    self.sessions=[NSMutableArray arrayWithArray:@[first,shared,last]];
    WHDClient *pending=first.client;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,200*NSEC_PER_MSEC),
                   dispatch_get_global_queue(QOS_CLASS_UTILITY,0), ^{ pending.finished=YES; });
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,400*NSEC_PER_MSEC),
                   dispatch_get_global_queue(QOS_CLASS_UTILITY,0), ^{ last.client.finished=YES; });
    [NSApp terminate:nil];
}
- (void)applicationWillTerminate:(NSNotification *)notification {
    for(WHDRemoteSession* session in self.sessions)if(!session.client.finished)abort();
    printf("PASS: application exits after both asynchronous workers (including a shared window) shut down\n"); fflush(stdout);
}
@end
int main(void) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        LifecycleApp *app=[LifecycleApp new]; NSApp.delegate=app;
        [NSApp run];
    }
    return 0;
}
