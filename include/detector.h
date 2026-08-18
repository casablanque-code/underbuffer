#ifndef UB_DETECTOR_H
#define UB_DETECTOR_H

#include <windows.h>

typedef WCHAR *(*ub_detector_fn)(const WCHAR *input);

typedef struct {
    const char *name;
    ub_detector_fn run;
} ub_detector_t;

extern const ub_detector_t UB_SYNC_PIPELINE[];
extern const size_t UB_SYNC_PIPELINE_COUNT;

/* Runs input through the full sync pipeline. Always returns a malloc'd
 * string (a copy of input if nothing changed), or NULL on OOM. */
WCHAR *ub_pipeline_run_sync(const WCHAR *input);

#endif
