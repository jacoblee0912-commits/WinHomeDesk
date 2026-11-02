add_library(freerdp-shadow-subsystem-impl STATIC surface_gdi.c surface_dxgi.c surface_wasapi.c surface_audio.c)
target_link_libraries(freerdp-shadow-subsystem-impl PRIVATE freerdp winpr gdi32 user32 advapi32 d3d11 dxgi dxguid d3dcompiler ole32 OpenSSL::Crypto)
