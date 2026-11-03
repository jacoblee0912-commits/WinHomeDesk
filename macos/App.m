/* WinHomeDesk macOS launcher and local protocol probe. Apache-2.0. */
#import <Cocoa/Cocoa.h>
#import <ApplicationServices/ApplicationServices.h>
#import <CommonCrypto/CommonKeyDerivation.h>
#import <CommonCrypto/CommonDigest.h>
#import <CommonCrypto/CommonCryptor.h>
#import <Security/Security.h>
#import "Client.h"
#import "VideoSettings.h"
#include "ConnectionCheck.h"
#include <sys/stat.h>

static NSString *hex(const unsigned char *bytes, NSUInteger count) {
    NSMutableString *s=[NSMutableString new]; for(NSUInteger i=0;i<count;i++) [s appendFormat:@"%02x",bytes[i]]; return s;
}
static void alert(NSString *message) { NSAlert *a=[NSAlert new]; a.messageText=message; [a runModal]; }
@interface WHDRemoteSession : NSObject
@property WHDClient* client;
@property WHDRemoteView* view;
@property NSWindow* window;
@property NSString* name;
@property NSArray* connectionKey;
@property NSPopUpButton* monitorChoice;
@property NSArray* lastMonitors;
@property NSString* lastStatus;
@property NSImage* lastImage;
@property BOOL closed;
@end
@implementation WHDRemoteSession
@end
@interface WHDApp : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property NSWindow *window;
@property NSTextField *host, *port, *user, *domain, *bind, *serverPort, *status;
@property NSSecureTextField *password, *serverPassword;
@property NSButton *consent, *startButton, *connectButton, *shareAudio;
@property NSTextView *log;
@property NSPopUpButton *quality, *fps, *captureMode, *graphicsMode, *encoderMode, *clientGraphics;
@property NSTextField *videoStatus;
@property NSArray<NSControl *> *videoControls;
@property NSTask *server;
@property NSMutableArray<WHDRemoteSession*>* sessions;
@property(weak) WHDRemoteSession* activeSession;
@property NSTimer *timer;
@property NSString *lastClientStatus;
@end
@implementation WHDApp
- (NSTextField *)field:(NSString *)label value:(NSString *)value y:(CGFloat)y secure:(BOOL)secure {
    NSTextField *l=[NSTextField labelWithString:label]; l.frame=NSMakeRect(24,y+3,200,24); [self.window.contentView addSubview:l];
    NSTextField *f=secure?[NSSecureTextField new]:[NSTextField new]; f.frame=NSMakeRect(230,y,420,26); f.stringValue=value;
    [self.window.contentView addSubview:f]; return f;
}
- (NSButton *)button:(NSString *)label action:(SEL)action x:(CGFloat)x y:(CGFloat)y width:(CGFloat)w {
    NSButton *b=[NSButton buttonWithTitle:label target:self action:action]; b.frame=NSMakeRect(x,y,w,32); [self.window.contentView addSubview:b]; return b;
}
- (NSPopUpButton *)choice:(NSString *)label items:(NSArray<NSString *> *)items key:(NSString *)key y:(CGFloat)y {
    NSTextField *title=[NSTextField labelWithString:label]; title.frame=NSMakeRect(690,y+31,330,22);
    [self.window.contentView addSubview:title];
    NSPopUpButton *popup=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(685,y,335,30) pullsDown:NO];
    [popup addItemsWithTitles:items]; popup.identifier=key;
    NSInteger saved=[NSUserDefaults.standardUserDefaults integerForKey:key];
    [popup selectItemAtIndex:saved>=0 && saved<items.count?saved:0];
    popup.target=self; popup.action=@selector(saveVideo:);
    [self.window.contentView addSubview:popup]; return popup;
}
- (void)saveVideo:(NSPopUpButton *)sender {
    [NSUserDefaults.standardUserDefaults setInteger:sender.indexOfSelectedItem forKey:sender.identifier];
}
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    self.sessions=[NSMutableArray new];
    NSMenu *menu=[NSMenu new]; NSMenuItem *root=[NSMenuItem new]; [menu addItem:root];
    NSMenu *appMenu=[NSMenu new]; [appMenu addItemWithTitle:@"Quit WinHomeDesk" action:@selector(terminate:) keyEquivalent:@"q"]; root.submenu=appMenu; NSApp.mainMenu=menu;
    NSMenuItem* windows=[NSMenuItem new];windows.title=@"창";windows.submenu=[[NSMenu alloc] initWithTitle:@"창"];
    NSMenuItem* settings=[windows.submenu addItemWithTitle:@"연결 설정 창" action:@selector(showSettings:) keyEquivalent:@"0"];settings.target=self;
    [windows.submenu addItem:NSMenuItem.separatorItem];[menu addItem:windows];NSApp.windowsMenu=windows.submenu;
    self.window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,1040,820) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable backing:NSBackingStoreBuffered defer:NO];
    self.window.title=@"WinHomeDesk — macOS (local preview)"; self.window.delegate=self;
    self.host=[self field:@"Windows 주소 / Host" value:@"" y:765 secure:NO];
    self.port=[self field:@"RDP 포트 / Port" value:@"3389" y:730 secure:NO];
    self.user=[self field:@"사용자 / User" value:@"" y:695 secure:NO];
    self.domain=[self field:@"도메인 / Domain (선택)" value:@"" y:660 secure:NO];
    self.password=(id)[self field:@"Windows 또는 앱 암호" value:@"" y:625 secure:YES];
    self.connectButton=[self button:@"Windows 연결" action:@selector(connect:) x:230 y:580 width:170];
    self.connectButton.toolTip=@"같은 PC는 아직 열지 않은 모니터를 새 창에서 엽니다. 각 원격 창 상단에서 모니터를 선택할 수 있습니다.";
    [self button:@"접속 진단" action:@selector(diagnose:) x:405 y:580 width:120];
    [self button:@"선택 창 닫기" action:@selector(disconnect:) x:530 y:580 width:120];
    self.bind=[self field:@"Mac 수신 IP / Listen IP" value:@"127.0.0.1" y:520 secure:NO];
    self.serverPort=[self field:@"Mac RDP 포트" value:@"3390" y:485 secure:NO];
    self.serverPassword=(id)[self field:@"공유 암호 (사용자 surface)" value:@"" y:450 secure:YES];
    self.consent=[NSButton checkboxWithTitle:@"현재 Mac 화면과 키보드·마우스 제어 공유에 동의합니다" target:nil action:nil];
    self.consent.frame=NSMakeRect(24,410,630,28); [self.window.contentView addSubview:self.consent];
    [self button:@"화면·입력 권한 요청" action:@selector(permissions:) x:24 y:365 width:205];
    self.startButton=[self button:@"Mac 서버 시작" action:@selector(start:) x:235 y:365 width:205];
    [self button:@"서버 중지" action:@selector(stop:) x:445 y:365 width:205];
    [self button:@"RDP 파일 저장" action:@selector(export:) x:24 y:325 width:205];
    self.shareAudio=[NSButton checkboxWithTitle:@"Mac 시스템 소리 공유 (마이크 제외)" target:nil action:nil];
    self.shareAudio.frame=NSMakeRect(240,325,410,28);self.shareAudio.state=NSControlStateValueOn;[self.window.contentView addSubview:self.shareAudio];
    self.status=[NSTextField wrappingLabelWithString:@"원격 창 상단에서 모니터를 선택합니다. Windows 서버의 모든 모니터 공유가 필요합니다. 암호는 저장되지 않습니다."];
    self.status.frame=NSMakeRect(24,270,625,50); [self.window.contentView addSubview:self.status];
    NSScrollView *scroll=[[NSScrollView alloc] initWithFrame:NSMakeRect(24,24,632,235)]; scroll.hasVerticalScroller=YES;
    self.log=[[NSTextView alloc] initWithFrame:scroll.bounds]; self.log.editable=NO; self.log.font=[NSFont monospacedSystemFontOfSize:11 weight:NSFontWeightRegular]; scroll.documentView=self.log; [self.window.contentView addSubview:scroll];
    self.quality=[self choice:@"Mac 전송 화질" items:@[@"균형 · 최대 1920",@"속도 · 최대 1440",@"원본 · 디스플레이 픽셀 해상도"] key:@"video.quality" y:715];
    self.fps=[self choice:@"목표 프레임 속도" items:@[@"20 FPS",@"30 FPS",@"45 FPS"] key:@"video.fps" y:640];
    self.captureMode=[self choice:@"Mac 캡처 방식" items:@[@"자동 · ScreenCaptureKit",@"호환 · 스크린샷 캡처"] key:@"video.capture" y:565];
    self.graphicsMode=[self choice:@"Mac 화면 전송 방식" items:whdGraphicsTitles() key:@"video.graphics" y:490];
    self.encoderMode=[self choice:@"H.264 영상 압축" items:@[@"GPU 우선 · 실패 시 CPU",@"CPU · OpenH264"] key:@"video.encoder" y:415];
    self.clientGraphics=[self choice:@"Windows 수신 방식 · 다음 연결에 적용" items:whdGraphicsTitles() key:@"video.clientGraphics" y:315];
    self.videoControls=@[self.quality,self.fps,self.captureMode,self.graphicsMode,self.encoderMode];
    NSTextField *help=[NSTextField wrappingLabelWithString:@"Mac 공유 설정은 서버 시작 전에 변경합니다. 자동 캡처는 실패 시 호환 캡처로 전환합니다.\n\nAVC444는 CPU로 압축합니다. Planar와 Bitmap은 H.264 압축 설정을 사용하지 않습니다. 수신 프로그램에 따라 호환 방식으로 협상될 수 있습니다.\n\n암호화된 RDP/TCP로 전송합니다."];
    help.frame=NSMakeRect(690,95,325,195); [self.window.contentView addSubview:help];
    self.videoStatus=[NSTextField wrappingLabelWithString:@"실제 캡처·압축·전송 방식은 시작 후 로그에 표시됩니다."];
    self.videoStatus.frame=NSMakeRect(690,24,325,65); [self.window.contentView addSubview:self.videoStatus];
    [self.window center]; [self.window makeKeyAndOrderFront:nil]; [NSApp activateIgnoringOtherApps:YES];
    self.timer=[NSTimer timerWithTimeInterval:1.0/60 repeats:YES block:^(NSTimer *t) {
        for(WHDRemoteSession* session in [self.sessions copy]) {
            if(!session.closed && session.lastMonitors!=session.client.monitors)[self updateMonitors:session];
            NSImage* image=session.client.image;
            if(!session.closed && image!=session.lastImage) {session.lastImage=image;[session.view setNeedsDisplay:YES];}
            if(![session.client.status isEqualToString:session.lastStatus]) {
                session.lastStatus=session.client.status;
                [self updateWindowTitle:session];
                if(session==self.activeSession)self.status.stringValue=session.lastStatus;
                [self appendLog:[NSString stringWithFormat:@"Client %@: %@\n",session.name,session.lastStatus]];
            }
            if(session.closed && (session.client.finished || [self hasOpenWindowForClient:session.client]))[self.sessions removeObject:session];
        }
        self.startButton.enabled=!self.server.running;
        for(NSControl *control in self.videoControls) control.enabled=!self.server.running;
        self.shareAudio.enabled=!self.server.running;
    }];
    [NSRunLoop.mainRunLoop addTimer:self.timer forMode:NSRunLoopCommonModes];
}
- (int)validPort:(NSTextField *)field {
    NSScanner *scan=[NSScanner scannerWithString:field.stringValue]; int p=0;
    if(![scan scanInt:&p] || !scan.isAtEnd || p<1 || p>65535) { alert(@"포트는 1–65535 정수여야 합니다."); return 0; } return p;
}
- (void)appendLog:(NSString *)line {
    [self.log.textStorage appendAttributedString:[[NSAttributedString alloc] initWithString:line]];
    if(self.log.string.length>64000) [self.log.textStorage deleteCharactersInRange:NSMakeRange(0,self.log.string.length-48000)];
    [self.log scrollRangeToVisible:NSMakeRange(self.log.string.length,0)];
}
- (WHDClient*)createClient {return [WHDClient new];}
- (void)showSettings:(id)sender { [self.window makeKeyAndOrderFront:nil]; }
- (void)updateWindowTitle:(WHDRemoteSession*)session {
    NSString* monitor=session.view.monitorIndex<0?@"전체 화면":[NSString stringWithFormat:@"모니터 %ld",(long)session.view.monitorIndex+1];
    session.window.title=[NSString stringWithFormat:@"WinHomeDesk — %@ — %@ — %@",session.name,monitor,session.client.status];
}
- (void)updateMonitors:(WHDRemoteSession*)session {
    NSArray* monitors=session.client.monitors;session.lastMonitors=monitors;
    [session.monitorChoice removeAllItems];[session.monitorChoice addItemWithTitle:@"전체 화면"];session.monitorChoice.lastItem.tag=-1;
    NSUInteger count=MAX(1,monitors.count);
    for(NSUInteger i=0;i<count;i++) {
        BOOL primary=i<monitors.count && [monitors[i][@"primary"] boolValue];
        [session.monitorChoice addItemWithTitle:[NSString stringWithFormat:@"모니터 %lu%@",(unsigned long)i+1,primary?@" (주 모니터)":@""]];session.monitorChoice.lastItem.tag=i;
    }
    if(session.view.monitorIndex>=count)session.view.monitorIndex=0;
    [session.monitorChoice selectItemWithTag:session.view.monitorIndex];
    session.monitorChoice.toolTip=count==1?@"서버가 화면 1개만 전송 중입니다. Windows에서 모든 모니터 공유를 켜고 서버를 재시작하세요.":@"이 창에서 볼 Windows 모니터를 선택합니다.";
    [session.view setNeedsDisplay:YES];[self updateWindowTitle:session];
}
- (void)selectMonitor:(NSPopUpButton*)sender {
    WHDRemoteSession* session=[self sessionForWindow:sender.window];[session.view releaseKeys];
    session.view.monitorIndex=sender.selectedItem.tag;[session.view setNeedsDisplay:YES];[self updateWindowTitle:session];
}
- (NSInteger)nextMonitorForClient:(WHDClient*)client {
    for(NSUInteger i=0;i<MAX(1,client.monitors.count);i++) {
        BOOL used=NO;for(WHDRemoteSession* s in self.sessions)if(!s.closed && s.client==client && s.view.monitorIndex==i)used=YES;
        if(!used)return i;
    }
    return 0;
}
- (void)newMonitorWindow:(NSButton*)sender {
    WHDRemoteSession* session=[self sessionForWindow:sender.window];
    if(session.client.finished || session.client.stop){[self connectionError:@"연결이 종료됐습니다. 설정 창에서 다시 연결하세요."];return;}
    [self openWindowForClient:session.client key:session.connectionKey name:session.name];
}
- (void)connectionError:(NSString*)message { alert(message); }
- (BOOL)hasOpenWindowForClient:(WHDClient*)client {
    for(WHDRemoteSession* session in self.sessions)if(!session.closed && session.client==client)return YES;
    return NO;
}
- (void)connect:(id)sender {
    int port=[self validPort:self.port]; if(!port) return;
    NSCharacterSet* whitespace=NSCharacterSet.whitespaceAndNewlineCharacterSet;
    NSString* host=[self.host.stringValue stringByTrimmingCharactersInSet:whitespace];
    NSString* user=[self.user.stringValue stringByTrimmingCharactersInSet:whitespace];
    NSString* domain=[self.domain.stringValue stringByTrimmingCharactersInSet:whitespace];
    if(!host.length || !user.length) { [self connectionError:@"Windows 주소와 사용자 이름을 입력하세요."]; return; }
    // WinHomeDesk servers allow one controller. Multiple views of the same
    // endpoint/account must share that authenticated transport, not log in again.
    NSArray* key=@[host.lowercaseString,@(port),user,domain];
    WHDClient* existing=nil;
    for(WHDRemoteSession* candidate in self.sessions) {
        if(!candidate.closed && !candidate.client.stop && !candidate.client.finished && [candidate.connectionKey isEqual:key]) {
            existing=candidate.client;break;
        }
    }
    if(!existing && !self.password.stringValue.length) {
        [self connectionError:@"새 연결에는 Windows 또는 WinHomeDesk 앱 암호를 입력하세요. 같은 PC의 열린 연결은 암호 없이 새 창으로 볼 수 있습니다."];
        return;
    }
    WHDClient* client=existing?:[self createClient];
    if(!existing)client.graphicsMode=self.clientGraphics.indexOfSelectedItem;
    [self openWindowForClient:client key:key name:[NSString stringWithFormat:@"%@:%d",host,port]];
    if(!existing)[client connectHost:host port:port user:user domain:domain password:self.password.stringValue];
    else [self appendLog:[NSString stringWithFormat:@"Client %@:%d: 기존 연결을 새 창에서 공유 / Shared connection\n",host,port]];
    self.password.stringValue=@"";
}
- (void)openWindowForClient:(WHDClient*)client key:(NSArray*)key name:(NSString*)name {
    WHDRemoteSession* session=[WHDRemoteSession new];session.client=client;session.name=name;session.connectionKey=key;
    session.window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,1100,700) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable|NSWindowStyleMaskMiniaturizable backing:NSBackingStoreBuffered defer:NO];
    session.window.delegate=self;session.window.releasedWhenClosed=NO;session.window.acceptsMouseMovedEvents=YES;
    session.view=[[WHDRemoteView alloc] initWithFrame:NSMakeRect(0,0,1100,664)];session.view.autoresizingMask=NSViewWidthSizable|NSViewHeightSizable;session.view.client=session.client;
    session.view.monitorIndex=[self nextMonitorForClient:client];[session.window.contentView addSubview:session.view];
    session.monitorChoice=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(12,668,270,26)];session.monitorChoice.autoresizingMask=NSViewMinYMargin;
    session.monitorChoice.target=self;session.monitorChoice.action=@selector(selectMonitor:);[session.window.contentView addSubview:session.monitorChoice];
    NSButton* next=[NSButton buttonWithTitle:@"다른 모니터 새 창" target:self action:@selector(newMonitorWindow:)];next.frame=NSMakeRect(290,666,180,30);next.autoresizingMask=NSViewMinYMargin;[session.window.contentView addSubview:next];
    [self updateMonitors:session];
    [self.sessions addObject:session];self.activeSession=session;
    [session.window center];[session.window cascadeTopLeftFromPoint:NSMakePoint(120+self.sessions.count*24,800-self.sessions.count*24)];
    [session.window makeKeyAndOrderFront:nil];[session.window makeFirstResponder:session.view];
}
- (void)diagnose:(NSButton *)sender {
    int port=[self validPort:self.port]; if(!port) return;
    NSString *host=[self.host.stringValue stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    if(!host.length) { alert(@"Windows 주소를 입력하세요."); return; }
    sender.enabled=NO;
    [self appendLog:[NSString stringWithFormat:@"Diagnostic: %@:%d (no credentials)\n",host,port]];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0), ^{
        WHDConnectionCheck check=whd_check_endpoint(host.UTF8String,port);
        NSString *message;
        if(check.rdp) message=[NSString stringWithFormat:@"TCP/RDP 응답 정상 (%@). 실제 로그인을 다시 시도하세요.",check.protocol==2?@"NLA":check.protocol==1?@"TLS":@"RDP"];
        else message=[NSString stringWithFormat:@"%s 진단 실패: %s (errno=%d). 서버 상태와 macOS 로컬 네트워크 권한을 확인하세요.",check.stage,strerror(check.error),check.error];
        dispatch_async(dispatch_get_main_queue(), ^{ sender.enabled=YES; self.status.stringValue=message; [self appendLog:[message stringByAppendingString:@"\n"]]; });
    });
}
- (WHDRemoteSession*)sessionForWindow:(NSWindow*)window {
    for(WHDRemoteSession* session in self.sessions)if(session.window==window)return session;return nil;
}
- (void)disconnect:(id)sender { [self.activeSession.window close]; }
- (void)windowWillClose:(NSNotification *)notification {
    WHDRemoteSession* session=[self sessionForWindow:notification.object];
    if(session){
        [session.view releaseKeys];session.closed=YES;
        if(![self hasOpenWindowForClient:session.client])session.client.stop=YES;
        if(self.activeSession==session){self.activeSession=nil;for(WHDRemoteSession* other in self.sessions)if(!other.closed)self.activeSession=other;}
    }
    else if(notification.object==self.window)[NSApp terminate:nil];
}
- (void)windowDidBecomeKey:(NSNotification*)notification { WHDRemoteSession* s=[self sessionForWindow:notification.object];if(s)self.activeSession=s; }
- (void)windowDidResignKey:(NSNotification *)notification { [[self sessionForWindow:notification.object].view releaseKeys]; }
- (void)permissions:(id)sender {
    CGRequestScreenCaptureAccess();
    NSDictionary *options=@{(__bridge NSString *)kAXTrustedCheckOptionPrompt:@YES};
    AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);
    [self appendLog:@"시스템 설정 > 개인정보 보호 및 보안에서 WinHomeDesk의 화면 기록 및 손쉬운 사용을 허용한 후 앱을 다시 실행하세요.\n"];
}
- (void)start:(id)sender {
    if(self.server.running) return;
    int port=[self validPort:self.serverPort]; if(!port) return;
    NSString *address=[self.bind.stringValue stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    if(!address.length || [address rangeOfCharacterFromSet:[[NSCharacterSet characterSetWithCharactersInString:@"0123456789."] invertedSet]].location!=NSNotFound) { alert(@"Mac의 IPv4 주소를 입력하세요 (로컬 시험: 127.0.0.1)."); return; }
    if(self.consent.state!=NSControlStateValueOn || self.serverPassword.stringValue.length<8) { alert(@"화면 공유에 동의하고 8자 이상의 앱 전용 암호를 입력하세요."); return; }
    if(!CGPreflightScreenCaptureAccess() || !AXIsProcessTrusted()) { [self permissions:nil]; return; }
    NSString *folder=[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/WinHomeDesk/macOS"];
    NSError *error=nil;
    if(![[NSFileManager defaultManager] createDirectoryAtPath:folder withIntermediateDirectories:YES attributes:@{NSFilePosixPermissions:@0700} error:&error]) { alert(error.localizedDescription); return; }
    chmod(folder.fileSystemRepresentation,0700);
    unsigned char salt[16], hashBytes[32], nt[CC_MD4_DIGEST_LENGTH];
    NSData *utf8=[self.serverPassword.stringValue dataUsingEncoding:NSUTF8StringEncoding];
    NSData *utf16=[self.serverPassword.stringValue dataUsingEncoding:NSUTF16LittleEndianStringEncoding];
    if(SecRandomCopyBytes(kSecRandomDefault,sizeof(salt),salt)!=errSecSuccess ||
       CCKeyDerivationPBKDF(kCCPBKDF2,utf8.bytes,utf8.length,salt,sizeof(salt),kCCPRFHmacAlgSHA1,200000,hashBytes,sizeof(hashBytes))!=kCCSuccess) { alert(@"암호 파생 실패"); return; }
    CC_MD4(utf16.bytes,(CC_LONG)utf16.length,nt); // NTLM compatibility; never persisted
    NSMutableDictionary *env=[NSProcessInfo.processInfo.environment mutableCopy];
    env[@"SURFACE_DESK_SALT"]=hex(salt,sizeof(salt)); env[@"SURFACE_DESK_HASH"]=hex(hashBytes,sizeof(hashBytes));
    env[@"WINHOME_DESK_NT_HASH"]=hex(nt,sizeof(nt)); env[@"SURFACE_DESK_DATA"]=folder;
    [env removeObjectForKey:@"WINHOMEDESK_TEST_PATTERN"];
    env[@"SURFACE_DESK_AUDIO"]=self.shareAudio.state==NSControlStateValueOn?@"1":@"0";
    [env addEntriesFromDictionary:whdVideoEnvironment(self.quality.indexOfSelectedItem,self.fps.indexOfSelectedItem,self.captureMode.indexOfSelectedItem,self.encoderMode.indexOfSelectedItem)];
    memset(hashBytes,0,sizeof(hashBytes)); memset(nt,0,sizeof(nt));
    self.server=[NSTask new]; self.server.executableURL=[NSURL fileURLWithPath:[NSBundle.mainBundle.bundlePath stringByAppendingPathComponent:@"Contents/MacOS/WinHomeDeskServer"]];
    NSMutableArray *arguments=[@[[NSString stringWithFormat:@"/port:%d",port],[@"/bind-address:" stringByAppendingString:address]] mutableCopy];
    [arguments addObjectsFromArray:whdServerGraphicsArguments(self.graphicsMode.indexOfSelectedItem)];
    self.server.arguments=arguments;
    self.videoStatus.stringValue=[NSString stringWithFormat:@"%@ · %@\n%@",self.quality.titleOfSelectedItem,self.fps.titleOfSelectedItem,self.graphicsMode.titleOfSelectedItem];
    [self appendLog:[NSString stringWithFormat:@"Settings: %@ / %@ / %@ / %@ / %@\n",self.quality.titleOfSelectedItem,self.fps.titleOfSelectedItem,self.captureMode.titleOfSelectedItem,self.graphicsMode.titleOfSelectedItem,self.encoderMode.titleOfSelectedItem]];
    self.server.environment=env;
    // NSTask retains this environment until the task object is released. Its
    // environment setter raises after launch, even when clearing our copy.
    self.serverPassword.stringValue=@"";
    NSPipe *pipe=[NSPipe pipe]; self.server.standardError=pipe; self.server.standardOutput=pipe;
    __weak WHDApp *weak=self;
    self.server.terminationHandler=^(NSTask *task) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if(weak.server==task) weak.server=nil;
        });
    };
    pipe.fileHandleForReading.readabilityHandler=^(NSFileHandle *handle) {
        NSData *data=handle.availableData;
        if(!data.length) { handle.readabilityHandler=nil; return; }
        NSString *s=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        if(s) dispatch_async(dispatch_get_main_queue(), ^{ [weak appendLog:s]; });
    };
    if(![self.server launchAndReturnError:&error]) {
        pipe.fileHandleForReading.readabilityHandler=nil;
        self.server=nil;
        alert(error.localizedDescription);
    }
}
- (void)stop:(id)sender { if(self.server.running) [self.server terminate]; }
- (void)export:(id)sender {
    int port=[self validPort:self.serverPort]; if(!port) return;
    NSString *address=self.bind.stringValue;
    if([address rangeOfCharacterFromSet:NSCharacterSet.newlineCharacterSet].location!=NSNotFound) return;
    NSSavePanel *panel=[NSSavePanel savePanel]; panel.nameFieldStringValue=@"WinHomeDesk-Mac.rdp";
    if([panel runModal]==NSModalResponseOK) {
        NSString *rdp=[NSString stringWithFormat:@"full address:s:%@:%d\r\nusername:s:surface\r\nprompt for credentials:i:1\r\nauthentication level:i:2\r\nenablecredsspsupport:i:1\r\nscreen mode id:i:1\r\nsession bpp:i:32\r\n",address,port];
        NSError *error=nil; if(![rdp writeToURL:panel.URL atomically:YES encoding:NSUTF16StringEncoding error:&error]) alert(error.localizedDescription);
    }
}
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    BOOL finished=YES;
    for(WHDRemoteSession* session in self.sessions){[session.view releaseKeys];session.client.stop=YES;if(!session.client.finished)finished=NO;}
    [self stop:nil];
    if (finished && !self.server.running) return NSTerminateNow;
    /* AppKit stops servicing default-mode timers after NSTerminateLater.
     * Retain the workers while waiting outside that run loop, then reply on main. */
    NSArray<WHDRemoteSession*>* pendingSessions=[self.sessions copy];
    NSTask *pendingServer=self.server;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0), ^{
        for(;;){BOOL done=!pendingServer.running;for(WHDRemoteSession* s in pendingSessions)if(!s.client.finished)done=NO;
            if(done)break;[NSThread sleepForTimeInterval:.05];}
        dispatch_async(dispatch_get_main_queue(), ^{ [NSApp replyToApplicationShouldTerminate:YES]; });
    });
    return NSTerminateLater;
}
@end

int main(int argc,const char *argv[]) {
    @autoreleasepool {
        umask(0077);
        if(argc==4 && strcmp(argv[1],"--diagnose")==0) {
            WHDConnectionCheck check=whd_check_endpoint(argv[2],atoi(argv[3]));
            printf("tcp=%d rdp=%d protocol=%d stage=%s errno=%d\n",check.connected,check.rdp,check.protocol,check.stage,check.error);
            return check.rdp?0:1;
        }
        if(argc==2 && strcmp(argv[1],"--doctor")==0) {
            printf("macOS=%s arch=arm64 screen=%d accessibility=%d\n",NSProcessInfo.processInfo.operatingSystemVersionString.UTF8String,CGPreflightScreenCaptureAccess(),AXIsProcessTrusted()); return 0;
        }
        if(argc>=5 && strcmp(argv[1],"--probe")==0) {
            NSString *host=@(argv[2]); int port=atoi(argv[3]);
            BOOL captureTest=argc>=6 && strcmp(argv[5],"--capture-test")==0;
            BOOL loopback=argc>=6 && (strcmp(argv[5],"--loopback-test")==0 || captureTest);
            if(port<1 || port>65535 || (loopback && ![host isEqualToString:@"127.0.0.1"])) return 2;
            char buffer[4096]={0}; if(!fgets(buffer,sizeof(buffer),stdin)) return 2;
            buffer[strcspn(buffer,"\r\n")]=0;
            WHDClient *client=[WHDClient new]; client.probe=YES; client.loopbackTest=loopback;
            client.captureTest=captureTest;
            client.audioProbe=loopback && getenv("WINHOMEDESK_PROBE_AUDIO")!=NULL;
            const char *graphics=getenv("WINHOMEDESK_PROBE_GRAPHICS");
            client.graphicsMode=graphics?atoi(graphics):3;
            client.tlsOnly=loopback && argc==7 && strcmp(argv[6],"--tls-test")==0;
            [client connectHost:host port:port user:@(argv[4]) domain:@"" password:@(buffer)]; memset(buffer,0,sizeof(buffer));
            NSDate *deadline=[NSDate dateWithTimeIntervalSinceNow:20];
            while(!client.finished && deadline.timeIntervalSinceNow>0) [NSThread sleepForTimeInterval:0.02];
            client.stop=YES;
            printf("frames=%lu pattern_frames=%lu width=%.0f height=%.0f finished=%d status=%s\n",client.frames,client.patternFrames,client.desktopSize.width,client.desktopSize.height,client.finished,client.status.UTF8String);
            return (loopback && !captureTest ? client.patternFrames>=3 : client.frames>=(captureTest?1:30)) && client.finished ? 0 : 1;
        }
        [NSApplication sharedApplication]; [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        WHDApp *delegate=[WHDApp new]; NSApp.delegate=delegate; [NSApp run];
    }
    return 0;
}
