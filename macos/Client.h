#import <Cocoa/Cocoa.h>
#include <freerdp/freerdp.h>
@interface WHDClient : NSObject
@property(atomic) BOOL stop, finished, connected;
@property(atomic) NSUInteger frames;
@property(nonatomic) NSInteger graphicsMode;
@property(atomic) BOOL gfxConnected;
@property(atomic) NSUInteger patternFrames;
@property(atomic, copy) NSString *status;
@property(atomic, strong) NSImage *image;
@property(atomic) NSSize desktopSize;
@property(atomic,copy) NSArray<NSDictionary*>* monitors;
- (BOOL)setRemoteMonitors:(const MONITOR_DEF*)monitors count:(UINT32)count;
@property(nonatomic) BOOL probe, loopbackTest, tlsOnly, captureTest;
@property(nonatomic) BOOL audioProbe;
@property(nonatomic) HANDLE inputWake;
@property(nonatomic,copy) void (^lastMovement)(rdpInput*);
@property(nonatomic, strong) NSMutableArray *events;
- (void)connectHost:(NSString *)host port:(int)port user:(NSString *)user domain:(NSString *)domain password:(NSString *)password;
- (void)enqueue:(void (^)(rdpInput *))event;
- (void)enqueueMovement:(void (^)(rdpInput *))event;
@end
@interface WHDRemoteView : NSView <NSTextInputClient>
@property(nonatomic, strong) WHDClient *client;
@property(nonatomic, strong) NSMutableSet<NSNumber *> *held;
@property(nonatomic, strong) NSMutableAttributedString *marked;
@property(nonatomic) NSInteger monitorIndex; // -1: entire desktop; otherwise server layout index
- (NSRect)sourceRect;
- (NSRect)imageRect;
- (NSPoint)remotePoint:(NSPoint)point;
- (void)releaseKeys;
@end
