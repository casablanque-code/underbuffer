#ifndef UB_AUTORUN_H
#define UB_AUTORUN_H

#include <windows.h>

/* Enforces the autorun state in the registry to match `enabled`,
 * unconditionally, every call -- config is the source of truth, no
 * separate read-compare-write dance. Returns TRUE on success. */
BOOL ub_autorun_apply(BOOL enabled);

#endif
