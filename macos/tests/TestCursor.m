#import "../../work/FreeRDP/server/shadow/Mac/mac_cursor_pixels.h"
#include <freerdp/server/shadow.h>
#include <freerdp/codec/color.h>
#include <assert.h>
int main(){@autoreleasepool{
    BYTE rgba[]={255,0,0,255, 0,255,0,255, 0,0,255,255, 0,0,0,0};
    CGColorSpaceRef color=CGColorSpaceCreateDeviceRGB();
    CGContextRef source=CGBitmapContextCreate(rgba,2,2,8,8,color,(CGBitmapInfo)kCGImageAlphaPremultipliedLast);
    CGImageRef cg=CGBitmapContextCreateImage(source);NSImage* image=[[NSImage alloc]initWithCGImage:cg size:NSMakeSize(2,2)];
    NSData* data=whd_cursor_pixels(image,2,2);assert(data.length==16);
    const BYTE* p=data.bytes;assert(p[2]==255 && p[0]==0 && p[8]==255 && p[10]==0);
    SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE packet={0};
    assert(shadow_subsystem_pointer_convert_alpha_pointer_data_to_format(p,PIXEL_FORMAT_BGRA32,TRUE,2,2,&packet)>0);
    BYTE restored[16]={0};
    assert(freerdp_image_copy_from_pointer_data(restored,PIXEL_FORMAT_BGRA32,8,0,0,2,2,
        packet.xorMaskData,packet.lengthXorMask,packet.andMaskData,packet.lengthAndMask,24,NULL));
    assert(!memcmp(restored,p,16));
    assert(!whd_cursor_pixels(image,0,2) && !whd_cursor_pixels(image,97,2));
    free(packet.xorMaskData);free(packet.andMaskData);CGImageRelease(cg);CGContextRelease(source);CGColorSpaceRelease(color);
    puts("PASS cursor: top/bottom orientation, RGB, transparency, RDP mask round-trip, size bounds");
}return 0;}
