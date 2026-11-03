/* Synchronous low-latency VideoToolbox output for the shared shadow codec worker.
 * The callback converts AVCC into Annex B; no B frames or unbounded frame queue.
 * Apache-2.0. */
#include "surface_vt.h"
#include <VideoToolbox/VideoToolbox.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

struct SurfaceVT {
    VTCompressionSessionRef session;
    UINT32 width, height, codedWidth, codedHeight, fps;
    int64_t frame;
    BYTE* bytes;
    size_t length, capacity;
    OSStatus status;
};
static BOOL append(SurfaceVT* s, const void* data, size_t size) {
    if (size > UINT32_MAX - s->length) return FALSE;
    size_t needed = s->length + size;
    if (needed > s->capacity) {
        BYTE* next = realloc(s->bytes, needed);
        if (!next) return FALSE;
        s->bytes = next; s->capacity = needed;
    }
    memcpy(s->bytes + s->length, data, size); s->length = needed;
    return TRUE;
}
static BOOL nal(SurfaceVT* s, const void* bytes, size_t size) {
    static const BYTE start[] = {0, 0, 0, 1};
    return append(s, start, sizeof(start)) && append(s, bytes, size);
}
static void output(void* context, void* source, OSStatus status,
                   VTEncodeInfoFlags flags, CMSampleBufferRef sample) {
    SurfaceVT* s = context;
    s->status = status;
    if (status || !sample || !CMSampleBufferDataIsReady(sample) ||
        (flags & kVTEncodeInfo_FrameDropped)) { s->status = status ? status : -1; return; }
    CMFormatDescriptionRef format = CMSampleBufferGetFormatDescription(sample);
    int header = 0;
    const uint8_t* parameter = NULL; size_t size = 0, count = 0;
    s->status = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(format, 0, &parameter,
                                                                 &size, &count, &header);
    if (s->status || header != 4) { s->status = -2; return; }
    CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sample, false);
    BOOL key = !attachments || !CFArrayGetCount(attachments) ||
        !CFDictionaryContainsKey(CFArrayGetValueAtIndex(attachments, 0), kCMSampleAttachmentKey_NotSync);
    if (key) for (size_t i = 0; i < count; i++) {
        s->status = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(format, i, &parameter,
                                                                     &size, NULL, NULL);
        if (s->status || !nal(s, parameter, size)) { s->status = -3; return; }
    }
    CMBlockBufferRef block = CMSampleBufferGetDataBuffer(sample);
    size_t total = CMBlockBufferGetDataLength(block), offset = 0;
    while (offset < total) {
        uint32_t big;
        if (total - offset < 4 || CMBlockBufferCopyDataBytes(block, offset, 4, &big)) { s->status = -4; return; }
        size_t n = CFSwapInt32BigToHost(big); offset += 4;
        if (!n || n > total - offset || s->length > UINT32_MAX - 4 || n > UINT32_MAX - s->length - 4) { s->status = -5; return; }
        /* CMBlockBuffer need not be contiguous. Reserve and copy each NAL safely. */
        static const BYTE start[] = {0, 0, 0, 1};
        if (!append(s, start, 4)) { s->status = -6; return; }
        size_t needed = s->length + n;
        if (needed > s->capacity) {
            BYTE* next = realloc(s->bytes, needed);
            if (!next) { s->status = -6; return; }
            s->bytes = next; s->capacity = needed;
        }
        s->status = CMBlockBufferCopyDataBytes(block, offset, n, s->bytes + s->length);
        if (s->status) return;
        s->length += n; offset += n;
    }
}
static OSStatus number(VTCompressionSessionRef session, CFStringRef key, int value) {
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberIntType, &value);
    if (!n) return -1;
    OSStatus status = VTSessionSetProperty(session, key, n); CFRelease(n); return status;
}
SurfaceVT* surface_vt_new(UINT32 width, UINT32 height, UINT32 fps, UINT32 bitrate, LONG* error) {
    SurfaceVT* s = calloc(1, sizeof(*s));
    *error = -1;
    if (!s || !width || !height || (width & 1) || (height & 1) || width > 16384 || height > 16384 || !fps) goto fail;
    s->width = width; s->height = height; s->fps = fps;
    /* MS-RDPEGFX 2.2.4.4 requires 16-aligned bitstream dimensions. Crop with
     * the RDP region mask, not an H.264 SPS cropping rectangle. */
    s->codedWidth = (width + 15u) & ~15u;
    s->codedHeight = (height + 15u) & ~15u;
    const void* keys[] = {kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder};
    const void* values[] = {kCFBooleanTrue};
    CFDictionaryRef spec = CFDictionaryCreate(NULL, keys, values, 1,
                                              &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    *error = VTCompressionSessionCreate(NULL, s->codedWidth, s->codedHeight, kCMVideoCodecType_H264, spec,
                                        NULL, NULL, output, s, &s->session);
    CFRelease(spec);
    if (*error) goto fail;
    if ((*error = VTSessionSetProperty(s->session, kVTCompressionPropertyKey_RealTime, kCFBooleanTrue)) ||
        (*error = VTSessionSetProperty(s->session, kVTCompressionPropertyKey_AllowFrameReordering, kCFBooleanFalse)) ||
        /* Match the Windows Media Foundation path: baseline, no decoder
         * reordering. RDP metadata applies to this exact access unit. */
        (*error = VTSessionSetProperty(s->session, kVTCompressionPropertyKey_ProfileLevel, kVTProfileLevel_H264_Baseline_AutoLevel)) ||
        (*error = number(s->session, kVTCompressionPropertyKey_ExpectedFrameRate, fps)) ||
        (*error = number(s->session, kVTCompressionPropertyKey_MaxKeyFrameInterval, fps * 2)) ||
        (*error = number(s->session, kVTCompressionPropertyKey_AverageBitRate, bitrate ? MIN(bitrate, INT_MAX) : 10000000)) ||
        (*error = VTCompressionSessionPrepareToEncodeFrames(s->session))) goto fail;
    CFTypeRef hardware = NULL;
    *error = VTSessionCopyProperty(s->session, kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder, NULL, &hardware);
    BOOL accelerated = hardware == kCFBooleanTrue;
    if (hardware) CFRelease(hardware);
    if (*error || !accelerated) { *error = *error ? *error : -7; goto fail; }
    return s;
fail:
    surface_vt_free(s); return NULL;
}
int surface_vt_encode(SurfaceVT* s, const BYTE* const planes[3], const UINT32 strides[3],
                      BYTE** data, UINT32* length, LONG* error) {
    CVPixelBufferRef pixel = NULL;
    *data = NULL; *length = 0;
    *error = CVPixelBufferCreate(NULL, s->codedWidth, s->codedHeight, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, NULL, &pixel);
    if (*error) return -1;
    *error = CVPixelBufferLockBaseAddress(pixel, 0);
    if (*error) { CVPixelBufferRelease(pixel); return -1; }
    BYTE* y = CVPixelBufferGetBaseAddressOfPlane(pixel, 0);
    BYTE* uv = CVPixelBufferGetBaseAddressOfPlane(pixel, 1);
    size_t ys = CVPixelBufferGetBytesPerRowOfPlane(pixel, 0), uvs = CVPixelBufferGetBytesPerRowOfPlane(pixel, 1);
    /* Replicate edge pixels into the coded padding, never read beyond the
     * caller's visible I420 planes (whose strides may be larger than width). */
    for (UINT32 row = 0; row < s->codedHeight; row++) {
        const BYTE* src = planes[0] + MIN(row, s->height - 1) * strides[0];
        memcpy(y + row * ys, src, s->width);
        memset(y + row * ys + s->width, src[s->width - 1], s->codedWidth - s->width);
    }
    for (UINT32 row = 0; row < s->codedHeight / 2; row++) for (UINT32 col = 0; col < s->codedWidth / 2; col++) {
        UINT32 sourceRow = MIN(row, s->height / 2 - 1), sourceCol = MIN(col, s->width / 2 - 1);
        uv[row * uvs + 2 * col] = planes[1][sourceRow * strides[1] + sourceCol];
        uv[row * uvs + 2 * col + 1] = planes[2][sourceRow * strides[2] + sourceCol];
    }
    CVPixelBufferUnlockBaseAddress(pixel, 0);
    s->length = 0; s->status = 0;
    CMTime pts = CMTimeMake(s->frame++, s->fps);
    *error = VTCompressionSessionEncodeFrame(s->session, pixel, pts, CMTimeMake(1, s->fps), NULL, NULL, NULL);
    CVPixelBufferRelease(pixel);
    if (!*error) *error = VTCompressionSessionCompleteFrames(s->session, pts);
    if (!*error) *error = s->status;
    if (*error || !s->length) return -1;
    *data = s->bytes; *length = (UINT32)s->length; return 1;
}
void surface_vt_free(SurfaceVT* s) {
    if (!s) return;
    if (s->session) { VTCompressionSessionInvalidate(s->session); CFRelease(s->session); }
    free(s->bytes); free(s);
}
