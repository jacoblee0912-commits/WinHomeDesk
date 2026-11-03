/* Apple hardware-only AVC420 encoder, with caller-owned CPU fallback. Apache-2.0. */
#ifndef SURFACE_VT_H
#define SURFACE_VT_H
#include <winpr/wtypes.h>
typedef struct SurfaceVT SurfaceVT;
SurfaceVT* surface_vt_new(UINT32 width, UINT32 height, UINT32 fps, UINT32 bitrate, LONG* error);
int surface_vt_encode(SurfaceVT* s, const BYTE* const planes[3], const UINT32 strides[3],
                      BYTE** data, UINT32* length, LONG* error);
void surface_vt_free(SurfaceVT* s);
#endif
