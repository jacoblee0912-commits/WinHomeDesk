/* Isolated local RDP input receiver. Never sends input until this window is key. */
#import "../Client.h"
#include <freerdp/input.h>
#import <ApplicationServices/ApplicationServices.h>
@interface Receiver : NSView
@property BOOL physical, ascii, unicode, down, up, drag, wheel;
@end
@implementation Receiver
- (BOOL)acceptsFirstResponder { return YES; }
- (void)drawRect:(NSRect)rect {
    [NSColor.windowBackgroundColor setFill]; NSRectFill(self.bounds);
    [@"WinHomeDesk RDP input test — isolated receiver\nKeyboard, Unicode, drag and wheel are captured only here." drawAtPoint:NSMakePoint(20,120) withAttributes:@{NSFontAttributeName:[NSFont systemFontOfSize:16]}];
}
- (void)keyDown:(NSEvent *)e {
    if(e.keyCode==97) self.physical=YES; // F6, independent of local Korean/English input source
    if([e.characters isEqualToString:@"a"]) self.ascii=YES;
    if([e.characters containsString:@"한"]) self.unicode=YES;
}
- (void)keyUp:(NSEvent *)e {}
- (void)mouseDown:(NSEvent *)e { self.down=YES; }
- (void)mouseUp:(NSEvent *)e { self.up=YES; }
- (void)mouseDragged:(NSEvent *)e { self.drag=YES; }
- (void)scrollWheel:(NSEvent *)e { self.wheel=YES; }
@end
@interface TestApp : NSObject <NSApplicationDelegate>
@property WHDClient *client;
@property NSWindow *window;
@property Receiver *receiver;
@property NSDate *deadline;
@property BOOL sent;
@property BOOL ending;
@property int result;
@end
@implementation TestApp
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    self.window=[[NSWindow alloc] initWithContentRect:NSMakeRect(100,100,650,240) styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
    self.window.title=@"WinHomeDesk isolated input test";
    self.receiver=[[Receiver alloc] initWithFrame:self.window.contentView.bounds]; self.window.contentView=self.receiver;
    [self.window makeKeyAndOrderFront:nil]; [self.window makeFirstResponder:self.receiver]; [NSApp activateIgnoringOtherApps:YES];
    self.deadline=[NSDate dateWithTimeIntervalSinceNow:15]; self.result=1;
    [NSTimer scheduledTimerWithTimeInterval:.05 repeats:YES block:^(NSTimer *timer) {
        if(self.ending) {
            if(self.client.finished) { [timer invalidate]; fflush(stdout); exit(self.result); }
            return;
        }
        if(!self.sent && self.window.keyWindow && self.client.frames>=30) {
            self.sent=YES;
            NSPoint center=[self.window convertPointToScreen:NSMakePoint(300,110)];
            CGRect display=CGDisplayBounds(CGMainDisplayID());
            NSSize remote=self.client.desktopSize;
            UINT16 x=(UINT16)(center.x/display.size.width*remote.width);
            UINT16 y=(UINT16)((display.size.height-center.y)/display.size.height*remote.height);
            [self.client enqueue:^(rdpInput *i) {
                freerdp_input_send_keyboard_event_ex(i,TRUE,FALSE,0x1e);
                freerdp_input_send_keyboard_event_ex(i,FALSE,FALSE,0x1e);
                freerdp_input_send_keyboard_event_ex(i,TRUE,FALSE,0x40); // F6
                freerdp_input_send_keyboard_event_ex(i,FALSE,FALSE,0x40);
                freerdp_input_send_unicode_keyboard_event(i,0,'a');
                freerdp_input_send_unicode_keyboard_event(i,KBD_FLAGS_RELEASE,'a');
                freerdp_input_send_unicode_keyboard_event(i,0,0xD55C);
                freerdp_input_send_unicode_keyboard_event(i,KBD_FLAGS_RELEASE,0xD55C);
                freerdp_input_send_mouse_event(i,PTR_FLAGS_MOVE,x,y);
                freerdp_input_send_mouse_event(i,PTR_FLAGS_BUTTON1|PTR_FLAGS_DOWN,x,y);
                freerdp_input_send_mouse_event(i,PTR_FLAGS_MOVE,x+15,y+15);
                freerdp_input_send_mouse_event(i,PTR_FLAGS_BUTTON1,x+15,y+15);
                freerdp_input_send_mouse_event(i,PTR_FLAGS_WHEEL|0x188,x+15,y+15);
            }];
        }
        Receiver *r=self.receiver;
        BOOL pass=r.physical && r.ascii && r.unicode && r.down && r.up && r.drag && r.wheel;
        if(pass || self.deadline.timeIntervalSinceNow<0 || self.client.finished) {
            self.result=pass?0:1; self.client.stop=YES;
            printf("physical=%d ascii=%d unicode=%d mouse_down=%d mouse_up=%d drag=%d wheel=%d\n",r.physical,r.ascii,r.unicode,r.down,r.up,r.drag,r.wheel);
            self.ending=YES;
        }
    }];
}
@end
int main(int argc,const char **argv) {
    @autoreleasepool {
        if(argc!=2) return 2;
        char password[256]; if(!fgets(password,sizeof(password),stdin)) return 2;
        password[strcspn(password,"\n")]=0;
        [NSApplication sharedApplication]; [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        TestApp *test=[TestApp new]; test.client=[WHDClient new];
        test.client.loopbackTest=YES; test.client.captureTest=YES;
        [test.client connectHost:@"127.0.0.1" port:atoi(argv[1]) user:@"surface" domain:@"" password:@(password)]; memset(password,0,sizeof(password));
        NSApp.delegate=test; [NSApp run]; return test.result;
    }
}
