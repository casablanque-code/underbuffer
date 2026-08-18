#include "netcheck.h"
#include "log.h"
#include <winhttp.h>
#include <stdlib.h>
#include <wchar.h>

#pragma comment(lib, "winhttp.lib")

/* WinHTTP, not WinINet: WinINet targets UI apps with a message loop
 * (auth dialogs, session-context dependence); WinHTTP is Microsoft's
 * recommended choice for services/daemons without a visible UI. */

typedef struct {
    WCHAR *url;
    DWORD seq_at_capture;
} netcheck_ctx_t;

static DWORD WINAPI netcheck_thread(LPVOID param)
{
    netcheck_ctx_t *ctx = (netcheck_ctx_t *)param;

    URL_COMPONENTS uc = { 0 };
    uc.dwStructSize = sizeof(uc);
    WCHAR host[256] = { 0 };
    WCHAR path[2048] = { 0 };
    uc.lpszHostName = host;
    uc.dwHostNameLength = ARRAYSIZE(host);
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = ARRAYSIZE(path);

    if (!WinHttpCrackUrl(ctx->url, 0, 0, &uc)) {
        ub_log(L"netcheck: WinHttpCrackUrl failed for %s (err=%lu)", ctx->url, GetLastError());
        goto cleanup;
    }

    HINTERNET hSession = WinHttpOpen(L"UnderBuffer/0.1",
                                      WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { ub_log(L"netcheck: WinHttpOpen failed"); goto cleanup; }

    WinHttpSetTimeouts(hSession, 3000, 3000, 3000, 3000);

    HINTERNET hConnect = WinHttpConnect(hSession, uc.lpszHostName, uc.nPort, 0);
    if (!hConnect) { ub_log(L"netcheck: WinHttpConnect failed"); WinHttpCloseHandle(hSession); goto cleanup; }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"HEAD", uc.lpszUrlPath,
                                             NULL, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { ub_log(L"netcheck: WinHttpOpenRequest failed"); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); goto cleanup; }

    BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (sent && WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD status = 0, status_len = sizeof(status);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_STATUS_CODE,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_len, WINHTTP_NO_HEADER_INDEX);
        ub_log(L"netcheck: %s -> HTTP %lu", ctx->url, status);
    } else {
        ub_log(L"netcheck: request failed for %s (err=%lu)", ctx->url, GetLastError());
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

cleanup:
    free(ctx->url);
    free(ctx);
    return 0;
}

void ub_netcheck_start_async(const WCHAR *url, DWORD clipboard_seq_at_capture)
{
    netcheck_ctx_t *ctx = (netcheck_ctx_t *)malloc(sizeof(netcheck_ctx_t));
    if (!ctx) return;
    ctx->url = _wcsdup(url);
    ctx->seq_at_capture = clipboard_seq_at_capture;
    if (!ctx->url) { free(ctx); return; }

    HANDLE h = CreateThread(NULL, 0, netcheck_thread, ctx, 0, NULL);
    if (h) {
        CloseHandle(h);
    } else {
        free(ctx->url);
        free(ctx);
    }
}
