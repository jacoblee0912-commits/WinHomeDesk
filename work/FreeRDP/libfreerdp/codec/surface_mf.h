/* Surface Desk hardware-only Media Foundation encoder. Apache-2.0. */
#ifndef SURFACE_MF_H
#define SURFACE_MF_H
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct SurfaceMF SurfaceMF;
SurfaceMF* surface_mf_new(UINT32 width,UINT32 height,UINT32 fps,UINT32 bitrate,HRESULT* error);
int surface_mf_encode(SurfaceMF* s,const BYTE* const planes[3],const UINT32 strides[3],BYTE** data,UINT32* length,HRESULT* error);
void surface_mf_free(SurfaceMF* s);
#ifdef __cplusplus
}
#endif
#endif
