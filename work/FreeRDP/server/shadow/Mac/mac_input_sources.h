/* WinHomeDesk macOS. Apache-2.0. Call Text Input Source Services on main. */
#ifndef WINHOMEDESK_MAC_INPUT_SOURCES_H
#define WINHOMEDESK_MAC_INPUT_SOURCES_H
#import <Carbon/Carbon.h>

static bool whd_source_is_korean(TISInputSourceRef source) {
    if (!source) return false;
    CFArrayRef languages = TISGetInputSourceProperty(source, kTISPropertyInputSourceLanguages);
    if (!languages || CFGetTypeID(languages) != CFArrayGetTypeID() || !CFArrayGetCount(languages)) return false;
    CFStringRef language = CFArrayGetValueAtIndex(languages, 0);
    return CFGetTypeID(language) == CFStringGetTypeID() &&
        (CFEqual(language, CFSTR("ko")) || CFStringHasPrefix(language, CFSTR("ko-")));
}
static bool whd_source_is_selectable(TISInputSourceRef source) {
    return source &&
        TISGetInputSourceProperty(source, kTISPropertyInputSourceIsEnabled) == kCFBooleanTrue &&
        TISGetInputSourceProperty(source, kTISPropertyInputSourceIsSelectCapable) == kCFBooleanTrue;
}
static TISInputSourceRef whd_copy_korean_source(void) {
    TISInputSourceRef preferred = TISCopyInputSourceForLanguage(CFSTR("ko"));
    if (whd_source_is_selectable(preferred) && whd_source_is_korean(preferred)) return preferred;
    if (preferred) CFRelease(preferred);
    /* macOS may return the parent input method for "ko". Only its input modes
     * are selectable. Search enabled keyboard sources, never enable a source. */
    const void *key = kTISPropertyInputSourceCategory;
    const void *value = kTISCategoryKeyboardInputSource;
    CFDictionaryRef filter = CFDictionaryCreate(NULL, &key, &value, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFArrayRef sources = TISCreateInputSourceList(filter, false);
    CFRelease(filter);
    TISInputSourceRef target = NULL;
    for (CFIndex i = 0; sources && i < CFArrayGetCount(sources); i++) {
        TISInputSourceRef source = (TISInputSourceRef)CFArrayGetValueAtIndex(sources, i);
        CFTypeRef category = TISGetInputSourceProperty(source, kTISPropertyInputSourceCategory);
        if (category && CFEqual(category, kTISCategoryKeyboardInputSource) &&
            whd_source_is_selectable(source) && whd_source_is_korean(source)) {
            target = (TISInputSourceRef)CFRetain(source);
            break;
        }
    }
    if (sources) CFRelease(sources);
    return target;
}
static OSStatus whd_toggle_korean_source(void) {
    TISInputSourceRef current = TISCopyCurrentKeyboardInputSource();
    if (!current) return paramErr;
    bool korean = whd_source_is_korean(current);
    TISInputSourceRef target = korean ? TISCopyCurrentASCIICapableKeyboardInputSource() :
        whd_copy_korean_source();
    if (korean && (!target || whd_source_is_korean(target))) {
        if (target) CFRelease(target);
        target = TISCopyInputSourceForLanguage(CFSTR("en"));
    }
    OSStatus result = paramErr;
    /* Only switch sources already enabled by the user. Never install/enable one. */
    if (whd_source_is_selectable(target) && !CFEqual(current, target) &&
        whd_source_is_korean(target) != korean) result = TISSelectInputSource(target);
    if (target) CFRelease(target);
    CFRelease(current);
    return result;
}
#endif
