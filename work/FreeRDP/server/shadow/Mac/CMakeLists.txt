add_compile_definitions(WITH_SHADOW_MAC)
add_library(freerdp-shadow-subsystem-impl STATIC mac_shadow.m mac_audio.m mac_cursor.m)
target_compile_options(freerdp-shadow-subsystem-impl PRIVATE -fobjc-arc)
target_link_libraries(freerdp-shadow-subsystem-impl PRIVATE
  "-framework Cocoa" "-framework AudioToolbox" "-framework ScreenCaptureKit" "-framework CoreMedia"
  "-framework CoreVideo" "-framework ApplicationServices" "-framework Carbon" ${OPENSSL_LIBRARIES})
