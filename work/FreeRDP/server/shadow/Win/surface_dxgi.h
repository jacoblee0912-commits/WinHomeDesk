/* Surface Desk, Apache-2.0. Primary-monitor Desktop Duplication capture. */
#ifndef SURFACE_DESK_DXGI_H
#define SURFACE_DESK_DXGI_H
#include <windows.h>
#include <stddef.h>
typedef struct SurfaceDxgi SurfaceDxgi;
SurfaceDxgi* surface_dxgi_create(int sourceWidth, int sourceHeight, int width, int height,
                                 HRESULT* error);
/* 1 = new image, 0 = unchanged, -1 = failure. Always releases acquired frames. */
int surface_dxgi_capture(SurfaceDxgi* capture, BYTE* pixels, size_t stride, HRESULT* error);
void surface_dxgi_free(SurfaceDxgi* capture);
#endif
