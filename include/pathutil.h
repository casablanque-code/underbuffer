#ifndef UB_PATHUTIL_H
#define UB_PATHUTIL_H

#include <windows.h>

/* All path building goes through this -- no manual "%s\\%s" or "%s/%s"
 * concatenation anywhere else in the codebase. */
BOOL ub_path_join(WCHAR *out, size_t out_cap, const WCHAR *base, const WCHAR *leaf);

/* %LOCALAPPDATA%\UnderBuffer, created if missing. */
BOOL ub_get_app_data_dir(WCHAR *out, size_t out_cap);

#endif
