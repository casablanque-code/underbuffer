#ifndef UB_CONFIG_H
#define UB_CONFIG_H

#include <windows.h>

typedef struct {
    BOOL netcheck_enabled;
    DWORD netcheck_timeout_ms;
    BOOL autorun_enabled;
    WCHAR **extra_trackers;
    size_t extra_tracker_count;
} ub_config_t;

void ub_config_defaults(ub_config_t *cfg);

/* Parses config file text already in memory (key=value lines, '#'
 * comments, blank lines ignored, unknown keys ignored). Overrides
 * whatever cfg already holds -- call ub_config_defaults() first.
 * Pure function, no I/O: unit-testable without touching disk. */
void ub_config_parse(const WCHAR *text, ub_config_t *cfg);

void ub_config_free(ub_config_t *cfg);

#endif
