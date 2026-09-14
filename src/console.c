#include "mcapture.h"

#define LINE_CAP     4096
#define LOG_SIZE_CAP (1024 * 1024)

static bool s_attach_tried;
static bool s_attached;

static bool usable(HANDLE h) {
    if (!h || h == INVALID_HANDLE_VALUE) return false;
    SetLastError(NO_ERROR);
    return GetFileType(h) != FILE_TYPE_UNKNOWN || GetLastError() == NO_ERROR;
}

static HANDLE std_handle(DWORD which) {
    HANDLE h = GetStdHandle(which);
    if (usable(h)) return h;

    if (!s_attach_tried) {
        s_attach_tried = true;
        s_attached     = AttachConsole(ATTACH_PARENT_PROCESS) != 0;
    }
    if (!s_attached) return NULL;

    h = GetStdHandle(which);
    if (usable(h)) return h;

    h = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;

    SetStdHandle(which, h);
    return h;
}

static void write_utf8(HANDLE h, const wchar_t *text, bool newline) {
    int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (n <= 0) return;

    char  stack[LINE_CAP];
    char *buf = (size_t)n + 1 <= sizeof stack ? stack : (char *)malloc((size_t)n + 1);
    if (!buf) return;

    WideCharToMultiByte(CP_UTF8, 0, text, -1, buf, n, NULL, NULL);

    DWORD length = (DWORD)(n - 1);
    if (newline) {
        buf[length++] = '\r';
        buf[length++] = '\n';
    }

    DWORD written = 0;
    WriteFile(h, buf, length, &written, NULL);
    if (buf != stack) free(buf);
}

static void write_line(DWORD which, const wchar_t *text) {
    HANDLE h = std_handle(which);
    if (!h) return;

    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) {
        DWORD written = 0;
        WriteConsoleW(h, text, (DWORD)wcslen(text), &written, NULL);
        WriteConsoleW(h, L"\r\n", 2, &written, NULL);
        return;
    }

    write_utf8(h, text, true);
}

static void format(wchar_t *out, size_t cap, const wchar_t *fmt, va_list ap) {
    int n = _vsnwprintf(out, cap - 1, fmt, ap);
    if (n < 0) n = (int)cap - 1;
    out[n < (int)cap ? n : (int)cap - 1] = L'\0';
}

static void append_log(const wchar_t *text) {
    wchar_t dir[MAX_PATH];
    DWORD   n = GetEnvironmentVariableW(L"LOCALAPPDATA", dir, MAX_PATH);
    if (n == 0 || n >= MAX_PATH - 32) {
        n = GetTempPathW(MAX_PATH, dir);
        if (n == 0 || n >= MAX_PATH - 32) return;
    }

    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH - 1, L"%ls\\mcapture", dir);
    path[MAX_PATH - 1] = L'\0';
    CreateDirectoryW(path, NULL);
    wcsncat(path, L"\\mcapture.log", MAX_PATH - wcslen(path) - 1);

    WIN32_FILE_ATTRIBUTE_DATA info;
    bool too_big = GetFileAttributesExW(path, GetFileExInfoStandard, &info) &&
                   (info.nFileSizeHigh > 0 || info.nFileSizeLow > LOG_SIZE_CAP);

    HANDLE h = CreateFileW(path, too_big ? GENERIC_WRITE : FILE_APPEND_DATA,
                           FILE_SHARE_READ, NULL,
                           too_big ? CREATE_ALWAYS : OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;

    SYSTEMTIME t;
    GetLocalTime(&t);

    wchar_t line[LINE_CAP];
    _snwprintf(line, LINE_CAP - 1, L"%04u-%02u-%02u %02u:%02u:%02u.%03u %ls",
               t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond,
               t.wMilliseconds, text);
    line[LINE_CAP - 1] = L'\0';

    write_utf8(h, line, true);
    CloseHandle(h);
}

void mc_print(const wchar_t *fmt, ...) {
    wchar_t text[LINE_CAP];
    va_list ap;
    va_start(ap, fmt);
    format(text, LINE_CAP, fmt, ap);
    va_end(ap);
    write_line(STD_OUTPUT_HANDLE, text);
}

void mc_eprint(const wchar_t *fmt, ...) {
    wchar_t text[LINE_CAP];
    va_list ap;
    va_start(ap, fmt);
    format(text, LINE_CAP, fmt, ap);
    va_end(ap);
    write_line(STD_ERROR_HANDLE, text);
}

void mc_error(const wchar_t *fmt, ...) {
    wchar_t message[LINE_CAP - 16];
    va_list ap;
    va_start(ap, fmt);
    format(message, LINE_CAP - 16, fmt, ap);
    va_end(ap);

    wchar_t text[LINE_CAP];
    _snwprintf(text, LINE_CAP - 1, L"mcapture: %ls", message);
    text[LINE_CAP - 1] = L'\0';

    write_line(STD_ERROR_HANDLE, text);
    append_log(text);
}
