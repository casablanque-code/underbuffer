#ifndef UB_NETCHECK_H
#define UB_NETCHECK_H

#include <windows.h>

/* Fire-and-forget HEAD check on a worker thread, never call from
 * WM_CLIPBOARDUPDATE directly -- this returns immediately.
 *
 * If the cleaned link turns out to be broken (definitive HTTP 4xx/5xx
 * after following redirects), the clipboard is reverted to
 * original_text -- some links only work with the tracking params we
 * stripped attached. The revert only happens if the clipboard
 * sequence still equals seq_after_write (nothing else has copied
 * anything since our cleanup write).
 *
 * A failed *request* (timeout, DNS failure, no connection) does NOT
 * trigger a revert -- that's too ambiguous to justify undoing the
 * user's clipboard (could be a flaky network, not a bad link). Only
 * a definitive bad-status response does. */
void ub_netcheck_start_async(HWND owner, const WCHAR *cleaned_url,
                              const WCHAR *original_text,
                              DWORD seq_after_write, DWORD timeout_ms);

/* Pure decision extracted for unit testing without a real network
 * call. request_succeeded=FALSE means no response was obtained at
 * all (timeout/DNS/connection failure) -- see rationale above. */
static inline BOOL ub_netcheck_should_revert(BOOL request_succeeded, DWORD http_status)
{
    return request_succeeded && http_status >= 400;
}

#endif
