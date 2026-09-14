#include "mcapture.h"

#include <shlobj.h>

#define ATTEMPT_MAX 1000

static bool screenshots_folder(wchar_t *out, size_t cap) {
    PWSTR pictures = NULL;
    if (FAILED(SHGetKnownFolderPath(&FOLDERID_Pictures, KF_FLAG_CREATE, NULL, &pictures)))
        return false;

    int n = _snwprintf(out, cap - 1, L"%ls\\Screenshots", pictures);
    CoTaskMemFree(pictures);
    if (n <= 0 || (size_t)n >= cap - 1) return false;
    out[n] = L'\0';

    return CreateDirectoryW(out, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool mc_resolve_output(const McOptions *opt, wchar_t *out, size_t cap) {
    if (opt->has_output) {
        DWORD n = GetFullPathNameW(opt->output, (DWORD)cap, out, NULL);
        if (n == 0 || n >= cap) {
            mc_error(L"could not resolve the path '%ls'", opt->output);
            return false;
        }
        return true;
    }

    wchar_t dir[MC_PATH_CAP];
    if (!screenshots_folder(dir, MC_PATH_CAP)) {
        mc_error(L"could not open the Screenshots folder in Pictures");
        return false;
    }

    SYSTEMTIME t;
    GetLocalTime(&t);

    for (int attempt = 1; attempt <= ATTEMPT_MAX; attempt++) {
        wchar_t name[64];
        if (!mc_default_filename(t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
                                 t.wSecond, attempt, name, 64))
            break;

        int n = _snwprintf(out, cap - 1, L"%ls\\%ls", dir, name);
        if (n <= 0 || (size_t)n >= cap - 1) break;
        out[n] = L'\0';

        if (GetFileAttributesW(out) == INVALID_FILE_ATTRIBUTES) return true;
    }

    mc_error(L"could not find a free file name in %ls", dir);
    return false;
}
