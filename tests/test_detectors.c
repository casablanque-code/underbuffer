/*
 * Unit tests for detector_url / detector_json / detector_unbreak / pipeline.
 *
 * Built and run NATIVELY (gcc, no mingw, no Windows) via
 * tests/compat/windows.h — see the comment there. Fast loop: edit
 * src/detector_*.c -> `make test` -> see results in seconds, no exe
 * rebuild and no manual copy-paste into Windows.
 *
 * Final validation before a release still has to go through the usual
 * `make` in WSL + a manual run of the exe on Windows (these tests don't
 * touch clipboard.c/main.c/netcheck.c — the actual Win32 integration).
 */
#include "detector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

WCHAR *ub_detect_url(const WCHAR *input);
WCHAR *ub_detect_json(const WCHAR *input);
WCHAR *ub_detect_unbreak(const WCHAR *input);

static int g_failed = 0;
static int g_total = 0;

static void check_eq(const char *test_name, const WCHAR *got, const WCHAR *expected)
{
    g_total++;
    if (got == NULL) {
        printf("[FAIL] %s: detector returned NULL (expected a change)\n", test_name);
        printf("        expected: %ls\n", expected);
        g_failed++;
        return;
    }
    if (wcscmp(got, expected) != 0) {
        printf("[FAIL] %s\n", test_name);
        printf("        expected: %ls\n", expected);
        printf("        got:      %ls\n", got);
        g_failed++;
        return;
    }
    printf("[ OK ] %s\n", test_name);
}

static void check_null(const char *test_name, const WCHAR *got)
{
    g_total++;
    if (got != NULL) {
        printf("[FAIL] %s: expected NULL (detector should not apply), got: %ls\n",
               test_name, got);
        g_failed++;
        return;
    }
    printf("[ OK ] %s\n", test_name);
}

/* ---- detector_url ---- */

static void test_url_bare_strips_trackers(void)
{
    WCHAR *r = ub_detect_url(
        L"https://www.youtube.com/watch?v=dQw4w9WgXcQ&utm_source=telegram&si=abc123xyz");
    check_eq("url: bare link, trackers stripped",
             r, L"https://www.youtube.com/watch?v=dQw4w9WgXcQ");
    free(r);
}

static void test_url_embedded_in_text(void)
{
    WCHAR *r = ub_detect_url(
        L"Check this out: https://www.youtube.com/watch?v=dQw4w9WgXcQ&utm_source=telegram&si=abc123xyz -- fire");
    check_eq("url: link embedded in arbitrary text also gets cleaned",
             r,
             L"Check this out: https://www.youtube.com/watch?v=dQw4w9WgXcQ -- fire");
    free(r);
}

static void test_url_markdown_wrapped(void)
{
    WCHAR *r = ub_detect_url(
        L"[link](https://example.com/page?id=1&utm_source=x&utm_medium=y)");
    check_eq("url: markdown wrapper preserved, only the query gets cleaned",
             r, L"[link](https://example.com/page?id=1)");
    free(r);
}

static void test_url_multiple_in_text(void)
{
    WCHAR *r = ub_detect_url(
        L"a https://a.com/?utm_source=x b https://b.com/?fbclid=y c");
    check_eq("url: multiple links in one text -- both get cleaned",
             r, L"a https://a.com/ b https://b.com/ c");
    free(r);
}

static void test_url_no_query_untouched(void)
{
    WCHAR *r = ub_detect_url(L"see https://example.com/page here");
    check_null("url: no query string -- detector does not apply", r);
}

static void test_url_no_url_untouched(void)
{
    WCHAR *r = ub_detect_url(L"just plain text with no links at all");
    check_null("url: text with no URL -- detector does not apply", r);
}

static void test_url_keeps_non_tracker_params(void)
{
    WCHAR *r = ub_detect_url(L"https://example.com/search?q=test&utm_source=x&page=2");
    check_eq("url: useful params kept, trackers stripped",
             r, L"https://example.com/search?q=test&page=2");
    free(r);
}

/* ---- detector_json ---- */

static void test_json_pretty_prints(void)
{
    WCHAR *r = ub_detect_json(L"{\"status\":\"ok\",\"code\":200,\"data\":[\"test1\",\"test2\"]}");
    check_eq("json: compact json gets pretty-printed",
             r,
             L"{\n  \"status\": \"ok\",\n  \"code\": 200,\n  \"data\": [\n    \"test1\",\n    \"test2\"\n  ]\n}");
    free(r);
}

static void test_json_rejects_plain_text(void)
{
    WCHAR *r = ub_detect_json(L"this isn't json, just {some text} with braces");
    check_null("json: non-json (doesn't start with { or [) is left alone", r);
}

/* ---- detector_unbreak ---- */

static void test_unbreak_single_newline_glues_with_space(void)
{
    WCHAR *r = ub_detect_unbreak(L"word\nword");
    check_eq("unbreak: single \\n between words -> space", r, L"word word");
    free(r);
}

static void test_unbreak_double_newline_preserved(void)
{
    WCHAR *r = ub_detect_unbreak(L"word\n\nword\nword");
    check_eq("unbreak: double \\n (paragraph boundary) kept, single \\n glued",
             r, L"word\n\nword word");
    free(r);
}

static void test_unbreak_hyphen_wordbreak(void)
{
    WCHAR *r = ub_detect_unbreak(L"exam-\nple");
    check_eq("unbreak: hyphenated word wrap gets glued without the hyphen",
             r, L"example");
    free(r);
}

static void test_unbreak_list_marker_not_glued(void)
{
    /* A list marker after \n is the only potential change here, so the
     * detector correctly returns NULL (nothing to do). */
    WCHAR *r = ub_detect_unbreak(L"item1\n- item2");
    check_null("unbreak: list marker after \\n is not glued (no change -> NULL)", r);
}

static void test_unbreak_list_marker_not_glued_with_other_change(void)
{
    /* Same list marker, but there's also a regular break nearby that
     * should get glued -- the list marker itself stays untouched. */
    WCHAR *r = ub_detect_unbreak(L"this\nwas\n- item");
    check_eq("unbreak: list marker stays put, neighboring break gets glued",
             r, L"this was\n- item");
    free(r);
}

static void test_unbreak_space_before_newline_regression(void)
{
    /* Regression: "word \nword" used to leave BOTH the space AND the
     * raw \n, because the "is the char before \n a letter?" check
     * looked directly at the space that was already written instead
     * of looking through it. */
    WCHAR *r = ub_detect_unbreak(L"word \nword");
    check_eq("unbreak: a space before \\n no longer lets the \\n leak through",
             r, L"word word");
    free(r);
}

static void test_unbreak_multi_spaces_collapse(void)
{
    WCHAR *r = ub_detect_unbreak(L"word   word\nword");
    check_eq("unbreak: runs of spaces collapse into one",
             r, L"word word word");
    free(r);
}

static void test_unbreak_no_single_newline_untouched(void)
{
    WCHAR *r = ub_detect_unbreak(L"word\n\nword");
    check_null("unbreak: only double \\n (already normal text) -- left alone", r);
}

static void test_unbreak_cyrillic_glues_without_locale(void)
{
    /* Regression: is_word_char() used to rely on iswalnum(), which
     * depends on the process locale. setlocale() is never called by
     * the app, so under the default "C" locale iswalnum() returns
     * FALSE for anything outside ASCII -- meaning line-unbreak would
     * silently do nothing on Russian text. */
    WCHAR *r = ub_detect_unbreak(L"\u0421\u043c\u043e\u0442\u0440\u0438\nvot");
    check_eq("unbreak: Cyrillic text is glued without needing setlocale()",
             r, L"\u0421\u043c\u043e\u0442\u0440\u0438 vot");
    free(r);
}

/* ---- pipeline (integration) ---- */

static void test_pipeline_json_passthrough(void)
{
    WCHAR *r = ub_pipeline_run_sync(L"{\"a\":1}");
    check_eq("pipeline: json gets formatted through the whole pipeline",
             r, L"{\n  \"a\": 1\n}");
    free(r);
}

static void test_pipeline_wrapped_url_with_softwrap(void)
{
    /* Realistic case from the bug report: a tracked link plus text with
     * a soft line break in the same buffer. unbreak should glue the
     * line first, and url should strip the trackers afterwards. */
    WCHAR *r = ub_pipeline_run_sync(
        L"Check this\nhttps://www.youtube.com/watch?v=dQw4w9WgXcQ&utm_source=telegram&si=abc123xyz");
    check_eq("pipeline: line break glued and trackers stripped in one pass",
             r, L"Check this https://www.youtube.com/watch?v=dQw4w9WgXcQ");
    free(r);
}

static void test_pipeline_unchanged_text_is_copy(void)
{
    WCHAR *r = ub_pipeline_run_sync(L"ordinary text, nothing to see here");
    check_eq("pipeline: if no detector applies, result is just a copy of the input",
             r, L"ordinary text, nothing to see here");
    free(r);
}

int main(void)
{
    test_url_bare_strips_trackers();
    test_url_embedded_in_text();
    test_url_markdown_wrapped();
    test_url_multiple_in_text();
    test_url_no_query_untouched();
    test_url_no_url_untouched();
    test_url_keeps_non_tracker_params();

    test_json_pretty_prints();
    test_json_rejects_plain_text();

    test_unbreak_single_newline_glues_with_space();
    test_unbreak_double_newline_preserved();
    test_unbreak_hyphen_wordbreak();
    test_unbreak_list_marker_not_glued();
    test_unbreak_list_marker_not_glued_with_other_change();
    test_unbreak_space_before_newline_regression();
    test_unbreak_multi_spaces_collapse();
    test_unbreak_no_single_newline_untouched();
    test_unbreak_cyrillic_glues_without_locale();

    test_pipeline_json_passthrough();
    test_pipeline_wrapped_url_with_softwrap();
    test_pipeline_unchanged_text_is_copy();

    printf("\n%d/%d passed\n", g_total - g_failed, g_total);
    return g_failed == 0 ? 0 : 1;
}
