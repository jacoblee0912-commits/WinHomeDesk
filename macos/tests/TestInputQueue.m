#import "../Client.h"
#include <winpr/synch.h>
#include <assert.h>
int main(void){@autoreleasepool{
    WHDClient* a=[WHDClient new];WHDClient* b=[WHDClient new];a.connected=b.connected=YES;
    NSMutableArray* seen=[NSMutableArray new];
    assert(WaitForSingleObject(a.inputWake,0)==WAIT_TIMEOUT);
    for(int i=0;i<1000;i++)[a enqueueMovement:^(rdpInput* unused){[seen addObject:@(i)];}];
    [a enqueue:^(rdpInput* unused){[seen addObject:@"click"]; }];
    for(int i=1000;i<2000;i++)[a enqueueMovement:^(rdpInput* unused){[seen addObject:@(i)];}];
    assert(WaitForSingleObject(a.inputWake,0)==WAIT_OBJECT_0);
    assert(WaitForSingleObject(b.inputWake,0)==WAIT_TIMEOUT && b.events.count==0);
    assert(a.events.count==3);
    for(void(^event)(rdpInput*) in a.events)event(NULL);
    assert(([seen isEqualToArray:@[@999,@"click",@1999]]));
    puts("PASS input queue: immediate wake, stale movement coalescing, click ordering, session isolation");
}return 0;}
