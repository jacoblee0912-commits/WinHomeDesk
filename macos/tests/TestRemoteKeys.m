/* View-level regression tests. No sockets or OS input events are posted. */
#import "../Client.h"

@interface KeyRecorder : WHDRemoteView
@property NSMutableArray<NSArray<NSNumber *> *> *keys;
@property NSUInteger interpreted;
@property BOOL clearComposition;
@end
@implementation KeyRecorder
- (instancetype)init {
    if((self=[super initWithFrame:NSMakeRect(0,0,100,100)])) _keys=[NSMutableArray new];
    return self;
}
- (void)sendKey:(unsigned)key down:(BOOL)down { [self.keys addObject:@[@(key),@(down)]]; }
- (void)interpretKeyEvents:(NSArray<NSEvent *> *)events {
    self.interpreted++;
    // Model the text system's delete command, including an IME consuming the
    // final composing character before invoking the selector callback.
    if(self.clearComposition) [self unmarkText];
    [self doCommandBySelector:@selector(deleteBackward:)];
}
@end

static NSEvent *key(NSEventType type, unsigned code, unichar character, NSUInteger flags, BOOL repeat) {
    NSString *text=[NSString stringWithCharacters:&character length:1];
    return [NSEvent keyEventWithType:type location:NSZeroPoint modifierFlags:flags timestamp:0
        windowNumber:0 context:nil characters:text charactersIgnoringModifiers:text isARepeat:repeat keyCode:code];
}
static void tap(KeyRecorder *view,unsigned code,unichar character,NSUInteger flags) {
    [view keyDown:key(NSEventTypeKeyDown,code,character,flags,NO)];
    [view keyUp:key(NSEventTypeKeyUp,code,character,flags,NO)];
}
static int failures;
static void check(BOOL passed,const char *name) {
    printf("%s: %s\n",passed?"PASS":"FAIL",name); if(!passed) failures++;
}
int main(void) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        KeyRecorder *v=[KeyRecorder new];
        tap(v,0x33,NSDeleteCharacter,0);
        check([v.keys isEqual:@[@[@0x33,@YES],@[@0x33,@NO]]] && v.interpreted==0,
              "Backspace 0x7F sends physical press/release");
        [v.keys removeAllObjects];
        [v keyDown:key(NSEventTypeKeyDown,0x33,NSDeleteCharacter,0,NO)];
        [v keyDown:key(NSEventTypeKeyDown,0x33,NSDeleteCharacter,0,YES)];
        [v keyDown:key(NSEventTypeKeyDown,0x33,NSDeleteCharacter,0,YES)];
        [v keyUp:key(NSEventTypeKeyUp,0x33,NSDeleteCharacter,0,NO)];
        check([v.keys isEqual:@[@[@0x33,@YES],@[@0x33,@YES],@[@0x33,@YES],@[@0x33,@NO]]] && !v.held.count,
              "held Backspace repeats and releases");
        [v.keys removeAllObjects];
        tap(v,0x75,NSDeleteFunctionKey,0);
        check([v.keys isEqual:@[@[@0x75,@YES],@[@0x75,@NO]]],"forward Delete remains distinct");
        [v.keys removeAllObjects];
        tap(v,0x33,NSDeleteCharacter,NSEventModifierFlagControl);
        check([v.keys isEqual:@[@[@0x33,@YES],@[@0x33,@NO]]],"Ctrl+Backspace remains physical");
        [v.keys removeAllObjects]; v.interpreted=0;
        [v setMarkedText:@"한" selectedRange:NSMakeRange(1,0) replacementRange:NSMakeRange(NSNotFound,0)];
        tap(v,0x33,NSDeleteCharacter,0);
        check(v.interpreted==1 && !v.keys.count,"composing Backspace stays with local IME");
        v.clearComposition=YES; v.interpreted=0;
        tap(v,0x33,NSDeleteCharacter,0);
        check(v.interpreted==1 && !v.keys.count && !v.hasMarkedText,
              "IME clearing final composing character does not delete remote text");
        v.clearComposition=NO; [v.keys removeAllObjects];
        tap(v,0x33,NSDeleteCharacter,0);
        check(v.keys.count==2,"Backspace resumes after composition");
        [v.keys removeAllObjects];
        [v doCommandBySelector:@selector(deleteBackward:)];
        [v doCommandBySelector:@selector(deleteForward:)];
        check([v.keys isEqual:@[@[@0x33,@YES],@[@0x33,@NO],@[@0x75,@YES],@[@0x75,@NO]]],
              "AppKit deletion selectors are forwarded");
        [v.keys removeAllObjects];
        [v keyDown:key(NSEventTypeKeyDown,0x33,NSDeleteCharacter,0,NO)];
        [v releaseKeys];
        check([v.keys isEqual:@[@[@0x33,@YES],@[@0x33,@NO]]] && !v.held.count,
              "focus loss releases held Backspace");
        return failures?1:0;
    }
}
