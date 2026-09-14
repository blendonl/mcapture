#include "cli.h"

#include <string.h>

typedef enum {
    OPT_SELECT,
    OPT_SCREEN,
    OPT_MONITOR,
    OPT_WINDOW,
    OPT_RECT,
    OPT_OUTPUT,
    OPT_NO_SAVE,
    OPT_NO_CLIPBOARD,
    OPT_DELAY,
    OPT_CURSOR,
    OPT_LIST,
    OPT_HELP,
    OPT_VERSION,
} OptionId;

typedef enum {
    VALUE_NONE,
    VALUE_REQUIRED,
    VALUE_OPTIONAL_NUMBER,
} ValueKind;

typedef struct {
    OptionId       id;
    const wchar_t *short_name;
    const wchar_t *long_name;
    ValueKind      value;
} OptionSpec;

static const OptionSpec OPTIONS[] = {
    { OPT_SELECT,       L"-s", L"--select",       VALUE_NONE            },
    { OPT_SCREEN,       L"-f", L"--screen",       VALUE_NONE            },
    { OPT_MONITOR,      L"-m", L"--monitor",      VALUE_OPTIONAL_NUMBER },
    { OPT_WINDOW,       L"-w", L"--window",       VALUE_NONE            },
    { OPT_RECT,         L"-r", L"--rect",         VALUE_REQUIRED        },
    { OPT_OUTPUT,       L"-o", L"--output",       VALUE_REQUIRED        },
    { OPT_NO_SAVE,      NULL,  L"--no-save",      VALUE_NONE            },
    { OPT_NO_CLIPBOARD, NULL,  L"--no-clipboard", VALUE_NONE            },
    { OPT_DELAY,        L"-d", L"--delay",        VALUE_REQUIRED        },
    { OPT_CURSOR,       L"-c", L"--cursor",       VALUE_NONE            },
    { OPT_LIST,         NULL,  L"--list",         VALUE_NONE            },
    { OPT_HELP,         L"-h", L"--help",         VALUE_NONE            },
    { OPT_VERSION,      L"-V", L"--version",      VALUE_NONE            },
};

#define OPTION_COUNT ((int)(sizeof(OPTIONS) / sizeof(OPTIONS[0])))

static int usage(wchar_t *error, size_t cap, const wchar_t *a,
                 const wchar_t *b, const wchar_t *c, const wchar_t *d) {
    if (!error || cap == 0) return MC_EXIT_USAGE;

    const wchar_t *parts[4] = { a, b, c, d };
    size_t         len      = 0;

    for (int i = 0; i < 4; i++) {
        const wchar_t *s = parts[i];
        while (s && *s && len + 1 < cap) error[len++] = *s++;
    }
    error[len] = L'\0';
    return MC_EXIT_USAGE;
}

static wchar_t ascii_lower(wchar_t c) {
    return (c >= L'A' && c <= L'Z') ? (wchar_t)(c - L'A' + L'a') : c;
}

static bool equals_ignoring_case(const wchar_t *a, const wchar_t *b) {
    while (*a && *b) {
        if (ascii_lower(*a) != ascii_lower(*b)) return false;
        a++;
        b++;
    }
    return *a == *b;
}

static bool is_number(const wchar_t *s) {
    if (!s || !*s) return false;
    for (; *s; s++)
        if (*s < L'0' || *s > L'9') return false;
    return true;
}

static bool parse_index(const wchar_t *s, int *out) {
    if (!is_number(s) || wcslen(s) > 3) return false;

    int value = 0;
    for (; *s; s++) value = value * 10 + (*s - L'0');
    *out = value;
    return true;
}

static const OptionSpec *find_option(const wchar_t *arg,
                                     const wchar_t **inline_value) {
    *inline_value = NULL;
    if (arg[0] != L'-') return NULL;

    if (arg[1] == L'-') {
        const wchar_t *eq  = wcschr(arg, L'=');
        size_t         len = eq ? (size_t)(eq - arg) : wcslen(arg);

        for (int i = 0; i < OPTION_COUNT; i++) {
            const wchar_t *name = OPTIONS[i].long_name;
            if (wcslen(name) == len && wcsncmp(name, arg, len) == 0) {
                if (eq) *inline_value = eq + 1;
                return &OPTIONS[i];
            }
        }
        return NULL;
    }

    for (int i = 0; i < OPTION_COUNT; i++) {
        const wchar_t *name = OPTIONS[i].short_name;
        if (name && wcscmp(name, arg) == 0) return &OPTIONS[i];
    }
    return NULL;
}

static bool is_region(OptionId id) {
    return id == OPT_SELECT || id == OPT_SCREEN || id == OPT_MONITOR ||
           id == OPT_WINDOW || id == OPT_RECT;
}

int mc_parse_args(int argc, wchar_t **argv, McOptions *opt,
                  wchar_t *error, size_t error_cap) {
    memset(opt, 0, sizeof *opt);
    opt->command   = MC_COMMAND_CAPTURE;
    opt->region    = MC_REGION_SELECT;
    opt->monitor   = MC_MONITOR_UNDER_CURSOR;
    opt->format    = MC_FORMAT_PNG;
    opt->save      = true;
    opt->clipboard = true;
    if (error && error_cap) error[0] = L'\0';

    const wchar_t *region_flag = NULL;
    bool           no_save     = false;

    for (int i = 1; i < argc; i++) {
        const wchar_t    *arg = argv[i];
        const wchar_t    *value;
        const OptionSpec *spec = find_option(arg, &value);

        if (!spec) {
            if (arg[0] == L'-' && arg[1] != L'\0')
                return usage(error, error_cap, L"unknown option '", arg, L"'", NULL);
            return usage(error, error_cap, L"unexpected argument '", arg, L"'", NULL);
        }

        if (spec->value == VALUE_NONE && value)
            return usage(error, error_cap, spec->long_name,
                         L" does not take a value", NULL, NULL);

        if (spec->value == VALUE_REQUIRED && !value) {
            if (i + 1 >= argc)
                return usage(error, error_cap, spec->long_name,
                             L" needs a value", NULL, NULL);
            value = argv[++i];
        }

        if (spec->value == VALUE_OPTIONAL_NUMBER && !value &&
            i + 1 < argc && is_number(argv[i + 1]))
            value = argv[++i];

        if (is_region(spec->id)) {
            if (region_flag)
                return usage(error, error_cap, L"choose one region: ",
                             region_flag, L" and ", spec->long_name);
            region_flag = spec->long_name;
        }

        switch (spec->id) {
        case OPT_HELP:
            opt->command = MC_COMMAND_HELP;
            return MC_EXIT_OK;

        case OPT_VERSION:
            opt->command = MC_COMMAND_VERSION;
            return MC_EXIT_OK;

        case OPT_LIST:
            opt->command = MC_COMMAND_LIST;
            break;

        case OPT_SELECT:
            opt->region = MC_REGION_SELECT;
            break;

        case OPT_SCREEN:
            opt->region = MC_REGION_SCREEN;
            break;

        case OPT_WINDOW:
            opt->region = MC_REGION_WINDOW;
            break;

        case OPT_MONITOR:
            opt->region = MC_REGION_MONITOR;
            if (value && !parse_index(value, &opt->monitor))
                return usage(error, error_cap, L"--monitor needs an index from --list, got '",
                             value, L"'", NULL);
            break;

        case OPT_RECT:
            opt->region = MC_REGION_RECT;
            if (!mc_rect_parse(value, &opt->rect))
                return usage(error, error_cap,
                             L"--rect needs X,Y,W,H with W and H above zero, got '",
                             value, L"'", NULL);
            break;

        case OPT_OUTPUT: {
            size_t len = wcslen(value);
            if (len == 0)
                return usage(error, error_cap, L"--output needs a path", NULL, NULL, NULL);
            if (len >= MC_PATH_CAP)
                return usage(error, error_cap, L"--output path is too long", NULL, NULL, NULL);
            if (!mc_format_from_path(value, &opt->format))
                return usage(error, error_cap, L"unsupported image type '", value,
                             L"': use .png, .jpg or .bmp", NULL);
            memcpy(opt->output, value, (len + 1) * sizeof(wchar_t));
            opt->has_output = true;
            break;
        }

        case OPT_NO_SAVE:
            no_save = true;
            break;

        case OPT_NO_CLIPBOARD:
            opt->clipboard = false;
            break;

        case OPT_DELAY:
            if (!mc_parse_delay_ms(value, &opt->delay_ms))
                return usage(error, error_cap,
                             L"--delay needs seconds between 0 and 3600, got '",
                             value, L"'", NULL);
            break;

        case OPT_CURSOR:
            opt->cursor = true;
            break;
        }
    }

    if (no_save) {
        if (opt->has_output)
            return usage(error, error_cap,
                         L"--no-save and --output contradict each other", NULL, NULL, NULL);
        if (!opt->clipboard)
            return usage(error, error_cap,
                         L"--no-save and --no-clipboard leave nothing to do", NULL, NULL, NULL);
        opt->save = false;
    }

    return MC_EXIT_OK;
}

bool mc_format_from_path(const wchar_t *path, McFormat *out) {
    if (!path || !out) return false;

    const wchar_t *dot = NULL;
    for (const wchar_t *p = path; *p; p++) {
        if (*p == L'.') dot = p;
        else if (*p == L'\\' || *p == L'/') dot = NULL;
    }
    if (!dot || dot[1] == L'\0') return false;

    const wchar_t *ext = dot + 1;
    if (equals_ignoring_case(ext, L"png"))  { *out = MC_FORMAT_PNG;  return true; }
    if (equals_ignoring_case(ext, L"jpg") ||
        equals_ignoring_case(ext, L"jpeg")) { *out = MC_FORMAT_JPEG; return true; }
    if (equals_ignoring_case(ext, L"bmp"))  { *out = MC_FORMAT_BMP;  return true; }
    return false;
}

bool mc_parse_delay_ms(const wchar_t *text, int *out) {
    if (!text || !out || !*text) return false;

    const wchar_t *p      = text;
    long long      whole  = 0;
    int            digits = 0;

    while (*p >= L'0' && *p <= L'9') {
        whole = whole * 10 + (*p - L'0');
        if (whole > MC_DELAY_MAX_MS / 1000) return false;
        p++;
        digits++;
    }

    long long millis = 0;
    if (*p == L'.') {
        p++;
        int place = 100;
        while (*p >= L'0' && *p <= L'9') {
            millis += (*p - L'0') * place;
            place /= 10;
            p++;
            digits++;
        }
    }

    if (*p != L'\0' || digits == 0) return false;

    long long total = whole * 1000 + millis;
    if (total > MC_DELAY_MAX_MS) return false;

    *out = (int)total;
    return true;
}

static size_t put_digits(wchar_t *out, size_t at, int value, int width) {
    for (int i = width - 1; i >= 0; i--) {
        out[at + (size_t)i] = (wchar_t)(L'0' + value % 10);
        value /= 10;
    }
    return at + (size_t)width;
}

static size_t put_text(wchar_t *out, size_t at, const wchar_t *s) {
    while (*s) out[at++] = *s++;
    return at;
}

bool mc_default_filename(int year, int month, int day,
                         int hour, int minute, int second, int attempt,
                         wchar_t *out, size_t cap) {
    if (!out || cap < 40) return false;
    if (year < 0 || year > 9999 || month < 1 || month > 12 || day < 1 ||
        day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
        second < 0 || second > 60 || attempt < 1 || attempt > 9999)
        return false;

    size_t at = put_text(out, 0, L"mcapture-");
    at = put_digits(out, at, year, 4);
    at = put_digits(out, at, month, 2);
    at = put_digits(out, at, day, 2);
    at = put_text(out, at, L"-");
    at = put_digits(out, at, hour, 2);
    at = put_digits(out, at, minute, 2);
    at = put_digits(out, at, second, 2);

    if (attempt > 1) {
        at = put_text(out, at, L"-");
        int width = attempt >= 1000 ? 4 : attempt >= 100 ? 3 : attempt >= 10 ? 2 : 1;
        at = put_digits(out, at, attempt, width);
    }

    at = put_text(out, at, L".png");
    out[at] = L'\0';
    return true;
}
