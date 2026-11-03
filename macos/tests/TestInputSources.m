/* Source-selection tests replace TIS calls; no system input source is changed. */
#import <Foundation/Foundation.h>
#import <Carbon/Carbon.h>
#include <assert.h>
static TISInputSourceRef currentSource, asciiSource, koreanSource, englishSource, selectedSource;
static CFArrayRef enabledSources;
static unsigned selections;
static OSStatus selectResult;
static TISInputSourceRef copy_source(TISInputSourceRef source) { return source ? (TISInputSourceRef)CFRetain(source) : NULL; }
static TISInputSourceRef fake_current(void) { return copy_source(currentSource); }
static TISInputSourceRef fake_ascii(void) { return copy_source(asciiSource); }
static TISInputSourceRef fake_language(CFStringRef language) {
    return copy_source(CFEqual(language, CFSTR("ko")) ? koreanSource : englishSource);
}
static void *fake_property(TISInputSourceRef source, CFStringRef key) {
    return (void *)CFDictionaryGetValue((CFDictionaryRef)source, key);
}
static OSStatus fake_select(TISInputSourceRef source) { selections++; selectedSource=source; return selectResult; }
static CFArrayRef fake_list(CFDictionaryRef properties, Boolean includeAll) {
    assert(!includeAll);
    assert(CFEqual(CFDictionaryGetValue(properties,kTISPropertyInputSourceCategory),kTISCategoryKeyboardInputSource));
    return enabledSources ? CFRetain(enabledSources) : NULL;
}
#define TISCopyCurrentKeyboardInputSource fake_current
#define TISCopyCurrentASCIICapableKeyboardInputSource fake_ascii
#define TISCopyInputSourceForLanguage fake_language
#define TISGetInputSourceProperty fake_property
#define TISSelectInputSource fake_select
#define TISCreateInputSourceList fake_list
#include "../../work/FreeRDP/server/shadow/Mac/mac_input_sources.h"
static NSDictionary *source(NSString *language, NSString *identifier, BOOL enabled, BOOL selectable) {
    return @{(__bridge NSString *)kTISPropertyInputSourceLanguages:@[language],
        (__bridge NSString *)kTISPropertyInputSourceCategory:(__bridge NSString *)kTISCategoryKeyboardInputSource,
        (__bridge NSString *)kTISPropertyInputSourceID:identifier,
        (__bridge NSString *)kTISPropertyInputSourceIsEnabled:@(enabled),
        (__bridge NSString *)kTISPropertyInputSourceIsSelectCapable:@(selectable)};
}
int main(void) {
    @autoreleasepool {
        NSDictionary *en=source(@"en",@"test.US",YES,YES), *ko=source(@"ko",@"test.Korean",YES,YES);
        NSDictionary *latin=source(@"en",@"test.Dvorak",YES,YES), *regional=source(@"ko-KR",@"test.KoreanRegional",YES,YES);
        NSDictionary *disabled=source(@"ko",@"test.Disabled",NO,YES), *parent=source(@"ko",@"test.Parent",YES,NO);
        currentSource=englishSource=(__bridge TISInputSourceRef)en;
        asciiSource=(__bridge TISInputSourceRef)latin;
        koreanSource=(__bridge TISInputSourceRef)ko;
        assert(whd_toggle_korean_source()==noErr && selectedSource==koreanSource);
        currentSource=koreanSource;
        assert(whd_toggle_korean_source()==noErr && selectedSource==asciiSource);
        currentSource=(__bridge TISInputSourceRef)regional;
        assert(whd_toggle_korean_source()==noErr && selectedSource==asciiSource);
        asciiSource=koreanSource;
        assert(whd_toggle_korean_source()==noErr && selectedSource==englishSource);
        currentSource=englishSource; koreanSource=NULL;
        assert(whd_toggle_korean_source()==paramErr && selections==4);
        koreanSource=(__bridge TISInputSourceRef)disabled;
        assert(whd_toggle_korean_source()==paramErr && selections==4);
        koreanSource=(__bridge TISInputSourceRef)parent;
        assert(whd_toggle_korean_source()==paramErr && selections==4);
        koreanSource=englishSource;
        assert(whd_toggle_korean_source()==paramErr && selections==4);
        currentSource=NULL;
        assert(whd_toggle_korean_source()==paramErr && selections==4);
        // macOS 27 returns the enabled but unselectable Korean parent for "ko".
        // The selectable 2-set mode is a separate entry in the enabled list.
        currentSource=englishSource; koreanSource=(__bridge TISInputSourceRef)parent;
        NSArray *available=@[en,parent,disabled,ko]; enabledSources=(__bridge CFArrayRef)available;
        assert(whd_toggle_korean_source()==noErr && selectedSource==(__bridge TISInputSourceRef)ko);
        koreanSource=NULL;
        assert(whd_toggle_korean_source()==noErr && selectedSource==(__bridge TISInputSourceRef)ko);
        koreanSource=(__bridge TISInputSourceRef)regional;
        assert(whd_toggle_korean_source()==noErr && selectedSource==koreanSource);
        koreanSource=(__bridge TISInputSourceRef)parent;
        NSArray *unavailable=@[en,parent,disabled]; enabledSources=(__bridge CFArrayRef)unavailable;
        assert(whd_toggle_korean_source()==paramErr && selections==7);
        NSMutableDictionary *palette=[ko mutableCopy];
        palette[(__bridge NSString *)kTISPropertyInputSourceCategory]=(__bridge NSString *)kTISCategoryPaletteInputSource;
        NSArray *wrongCategory=@[palette]; enabledSources=(__bridge CFArrayRef)wrongCategory;
        assert(whd_toggle_korean_source()==paramErr && selections==7);
        enabledSources=(__bridge CFArrayRef)available; selectResult=paramErr;
        assert(whd_toggle_korean_source()==paramErr && selections==8);
        puts("PASS: 15 input-source selection cases (mock TIS; system sources unchanged)");
    }
}
