#ifndef UB_NETCHECK_H
#define UB_NETCHECK_H

#include <windows.h>

/* Fire-and-forget HEAD check on a worker thread. Never call from
 * WM_CLIPBOARDUPDATE directly -- this returns immediately. Result is
 * only logged for now; does not rewrite the clipboard. */
void ub_netcheck_start_async(const WCHAR *url, DWORD clipboard_seq_at_capture);

#endif
