/* Surface Desk diagnostic counters. Apache-2.0.
 * Each instance belongs to one worker; no global or cross-thread counters.
 * Times are wall-clock durations, not CPU utilization or client display latency.
 */
#ifndef SURFACE_DESK_PERF_H
#define SURFACE_DESK_PERF_H
#include <winpr/sysinfo.h>
#include <stdio.h>
#include <string.h>

static inline double surface_perf_now(void)
{
#ifdef _WIN32
    LARGE_INTEGER counter, frequency;
    if (QueryPerformanceFrequency(&frequency) && QueryPerformanceCounter(&counter))
        return (double)counter.QuadPart * 1000.0 / (double)frequency.QuadPart;
#endif
    return (double)GetTickCount64();
}

typedef struct {
    double start, encodeMs, sendMs, frameMaxMs, sendMaxMs, areaPct;
    UINT64 frames, payloadBytes, failed, other;
    const char* codec;
} SurfaceDeskClientPerf;

static inline void surface_perf_codec(SurfaceDeskClientPerf* p, const char* codec)
{
    if (!p->codec || strcmp(p->codec, codec) != 0) {
        p->codec = codec;
        fprintf(stderr, "SurfaceDesk CODEC name=%s\n", codec);
        fflush(stderr);
    }
}

static inline void surface_perf_frame(SurfaceDeskClientPerf* p, const char* codec,
                                      double begin, double sending, UINT64 bytes, double area)
{
    const double now = surface_perf_now();
    const double send = now - sending, frame = now - begin;
    surface_perf_codec(p, codec);
    p->frames++; p->encodeMs += sending - begin; p->sendMs += send;
    p->payloadBytes += bytes; p->areaPct += area;
    if (frame > p->frameMaxMs) p->frameMaxMs = frame;
    if (send > p->sendMaxMs) p->sendMaxMs = send;
}

static inline void surface_perf_report_client(SurfaceDeskClientPerf* p, BOOL closed)
{
    if (p->start <= 0) return;
    const double now = surface_perf_now();
    const double elapsed = now - p->start;
    if (!closed && elapsed < 2000.0) return;
    const double seconds = elapsed > 0 ? elapsed / 1000.0 : 1.0;
    const double n = p->frames ? (double)p->frames : 1.0;
    fprintf(stderr,
        "SurfaceDesk PERF_SEND state=%s window_s=%.3f frames=%llu fps=%.2f "
        "encode_ms=%.3f send_ms=%.3f send_max_ms=%.3f frame_max_ms=%.3f "
        "payload_bytes=%llu payload_mbps=%.3f area_pct=%.2f failed=%llu other=%llu\n",
        closed ? "closed" : "active", seconds, (unsigned long long)p->frames,
        p->frames / seconds, p->encodeMs / n, p->sendMs / n, p->sendMaxMs,
        p->frameMaxMs, (unsigned long long)p->payloadBytes,
        p->payloadBytes * 8.0 / seconds / 1000000.0, p->areaPct / n,
        (unsigned long long)p->failed, (unsigned long long)p->other);
    fflush(stderr);
    const char* codec = p->codec;
    memset(p, 0, sizeof(*p));
    p->codec = codec;
    p->start = closed ? 0 : now;
}
#endif
