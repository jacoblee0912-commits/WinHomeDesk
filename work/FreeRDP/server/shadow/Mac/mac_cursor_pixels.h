#pragma once
#import <Cocoa/Cocoa.h>
/* Top-down BGRA input for FreeRDP's bottom-up pointer mask conversion. */
static NSData* whd_cursor_pixels(NSImage* image,unsigned w,unsigned h) {
    if(!image || !w || !h || w>96 || h>96)return nil;
    NSMutableData* data=[NSMutableData dataWithLength:w*h*4];
    CGColorSpaceRef color=CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx=CGBitmapContextCreate(data.mutableBytes,w,h,8,w*4,color,kCGBitmapByteOrder32Little|kCGImageAlphaPremultipliedFirst);
    CGColorSpaceRelease(color);if(!ctx)return nil;
    CGRect proposed=CGRectMake(0,0,w,h);CGImageRef cg=[image CGImageForProposedRect:&proposed context:nil hints:nil];
    if(cg)CGContextDrawImage(ctx,CGRectMake(0,0,w,h),cg);
    CGContextRelease(ctx);return cg?data:nil;
}
