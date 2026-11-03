/* Windows-compatible graphics choices, with native macOS capture backends. Apache-2.0. */
#import <Foundation/Foundation.h>
static NSArray<NSString *> *whdGraphicsTitles(void) {
    return @[@"자동 · H.264 AVC420 우선", @"글자 선명 · H.264 AVC444 우선", @"무손실 색상 · GFX Planar", @"호환 · Bitmap"];
}
static NSArray<NSString *> *whdServerGraphicsArguments(NSInteger mode) {
    NSMutableArray *args=[@[@"-rfx",@"-nsc",@"+bitmap-compat",@"-gfx-progressive"] mutableCopy];
    [args addObjectsFromArray:mode==3 ? @[@"-gfx",@"-gfx-avc420",@"-gfx-avc444"] :
        @[@"+gfx",@"+gfx-planar",mode==2?@"-gfx-avc420":@"+gfx-avc420",mode==1?@"+gfx-avc444":@"-gfx-avc444"]];
    return args;
}
static NSDictionary<NSString *,NSString *> *whdVideoEnvironment(NSInteger quality, NSInteger fps,
                                                               NSInteger capture, NSInteger encoder) {
    return @{@"SURFACE_DESK_MAX_EDGE":quality==1?@"1440":quality==2?@"0":@"1920",
             @"SURFACE_DESK_FPS":fps==1?@"30":fps==2?@"45":@"20",
             @"SURFACE_DESK_CAPTURE":capture==1?@"screenshot":@"auto",
             @"SURFACE_DESK_GPU":encoder==1?@"0":@"1"};
}
