/* Single-producer/single-consumer latest-frame mailbox. Apache-2.0.
 * The consumer owns its snapshot until its next take. Pending frames are replaced,
 * never queued. No encoder or network call runs while holding this lock. */
#ifndef SURFACE_LATEST_H
#define SURFACE_LATEST_H
#include <windows.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    CRITICAL_SECTION lock;
    BYTE* pendingBuffer;
    BYTE* snapshot;
    size_t bytes;
    BOOL pending;
    UINT64 sequence, taken, replaced;
    double capturedAt;
    HANDLE ready;
} SurfaceLatest;
static SurfaceLatest* surface_latest_new(size_t bytes) {
    SurfaceLatest* m=calloc(1,sizeof(*m));if(!m)return NULL;
    InitializeCriticalSection(&m->lock);m->bytes=bytes;
    m->pendingBuffer=calloc(1,bytes);m->snapshot=calloc(1,bytes);
    m->ready=CreateEvent(NULL,FALSE,FALSE,NULL);
    if(!m->pendingBuffer || !m->snapshot || !m->ready) {
        free(m->pendingBuffer);free(m->snapshot);if(m->ready)CloseHandle(m->ready);
        DeleteCriticalSection(&m->lock);free(m);return NULL;
    }
    return m;
}
static BOOL surface_latest_put(SurfaceLatest* m,const BYTE* data,double capturedAt) {
    EnterCriticalSection(&m->lock);
    BOOL replaced=m->pending;
    memcpy(m->pendingBuffer,data,m->bytes);
    m->capturedAt=capturedAt;m->sequence++;m->pending=TRUE;
    if(replaced)m->replaced++;
    SetEvent(m->ready);LeaveCriticalSection(&m->lock);return replaced;
}
static BOOL surface_latest_take(SurfaceLatest* m,BYTE** data,UINT64* sequence,double* capturedAt) {
    EnterCriticalSection(&m->lock);
    BOOL available=m->pending;
    if(available) {
        BYTE* tmp=m->snapshot;m->snapshot=m->pendingBuffer;m->pendingBuffer=tmp;
        *data=m->snapshot;*sequence=m->sequence;*capturedAt=m->capturedAt;
        m->pending=FALSE;m->taken++;ResetEvent(m->ready);
    }
    LeaveCriticalSection(&m->lock);return available;
}
static void surface_latest_free(SurfaceLatest* m) {
    if(!m)return;
    CloseHandle(m->ready);DeleteCriticalSection(&m->lock);
    free(m->pendingBuffer);free(m->snapshot);free(m);
}
#endif
