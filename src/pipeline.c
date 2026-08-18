#include "detector.h"
#include <stdlib.h>
#include <wchar.h>

WCHAR *ub_detect_url(const WCHAR *input);
WCHAR *ub_detect_json(const WCHAR *input);
WCHAR *ub_detect_unbreak(const WCHAR *input);

const ub_detector_t UB_SYNC_PIPELINE[] = {
    { "unbreak", ub_detect_unbreak },
    { "json",    ub_detect_json    },
    { "url",     ub_detect_url     },
};
const size_t UB_SYNC_PIPELINE_COUNT = sizeof(UB_SYNC_PIPELINE) / sizeof(UB_SYNC_PIPELINE[0]);

WCHAR *ub_pipeline_run_sync(const WCHAR *input)
{
    WCHAR *current = _wcsdup(input);
    if (!current) return NULL;

    for (size_t i = 0; i < UB_SYNC_PIPELINE_COUNT; i++) {
        WCHAR *next = UB_SYNC_PIPELINE[i].run(current);
        if (next != NULL) {
            free(current);
            current = next;
        }
    }
    return current;
}
