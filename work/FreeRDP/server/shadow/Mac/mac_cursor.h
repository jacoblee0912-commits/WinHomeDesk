#import <Cocoa/Cocoa.h>
#include <freerdp/server/shadow.h>
@interface WHDCursor : NSObject
- (instancetype)initWithSubsystem:(rdpShadowSubsystem*)base bounds:(CGRect)bounds;
- (void)start;
- (void)stop;
@end
