/* Surface Desk, Apache-2.0 */
#ifndef SURFACE_AUDIO_H
#define SURFACE_AUDIO_H
#include <freerdp/server/shadow.h>
BOOL surface_audio_enabled(void);
void surface_audio_formats(rdpShadowSubsystem* subsystem);
DWORD WINAPI surface_audio_thread(LPVOID subsystem);
#endif
