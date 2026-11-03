/* ScreenCaptureKit -> bounded rdpsnd PCM queue. Apache-2.0. */
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#include <freerdp/server/shadow.h>
@interface WHDAudio : NSObject <SCStreamOutput, SCStreamDelegate>
- (instancetype)initWithSubsystem:(rdpShadowSubsystem *)base;
- (BOOL)startWithFilter:(SCContentFilter *)filter;
- (void)stop;
- (void)syntheticTick; /* Only called by the loopback-only test-pattern backend. */
@end
