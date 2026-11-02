/* Surface Desk, Apache-2.0. Caller owns COM initialization and all calls on one thread. */
#ifndef SURFACE_WASAPI_H
#define SURFACE_WASAPI_H
#include <windows.h>
#include <stdint.h>
typedef struct SurfaceWasapi SurfaceWasapi;
SurfaceWasapi* surface_wasapi_open(HRESULT* error);
/* Fixed 44100 Hz, stereo, signed 16-bit PCM. 0 = no packet, 1 = copied, -1 = failure. */
int surface_wasapi_read(SurfaceWasapi* s, int16_t* pcm, UINT32 capacityFrames,
                       UINT32* frames, BOOL* discontinuity, HRESULT* error);
UINT64 surface_wasapi_timestamp(SurfaceWasapi* s);
BOOL surface_wasapi_timestamp_valid(SurfaceWasapi* s);
BOOL surface_wasapi_default_changed(SurfaceWasapi* s);
void surface_wasapi_close(SurfaceWasapi* s);
#endif
