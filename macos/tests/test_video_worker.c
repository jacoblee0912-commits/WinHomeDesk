/* Regression: WinPR POSIX events must not execute the same codec job twice. */
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include "../../work/FreeRDP/server/shadow/surface_worker.h"
static atomic_uint calls;
static void encode(void *value) { atomic_fetch_add(&calls,1); Sleep(2); }
int main(void) {
    SurfaceWorker *worker=surface_worker_new(); assert(worker);
    for(unsigned i=1;i<=20;i++) {
        assert(surface_worker_start(worker,encode,NULL));
        assert(WaitForSingleObject(worker->done,2000)==WAIT_OBJECT_0);
        Sleep(3); assert(atomic_load(&calls)==i);
    }
    surface_worker_free(worker);
    puts("PASS: codec worker executes each job exactly once and shuts down");
}
