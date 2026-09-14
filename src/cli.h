#ifndef MCAPTURE_CLI_H
#define MCAPTURE_CLI_H

#include <stdbool.h>
#include <stddef.h>
#include <wchar.h>

#include "geom.h"

#define MC_EXIT_OK        0
#define MC_EXIT_FAILURE   1
#define MC_EXIT_USAGE     2
#define MC_EXIT_CANCELLED 3

#define MC_PATH_CAP  1024
#define MC_ERROR_CAP 512

#define MC_MONITOR_UNDER_CURSOR (-1)
#define MC_DELAY_MAX_MS         3600000

typedef enum {
    MC_COMMAND_CAPTURE = 0,
    MC_COMMAND_LIST,
    MC_COMMAND_HELP,
    MC_COMMAND_VERSION,
} McCommand;

typedef enum {
    MC_REGION_SELECT = 0,
    MC_REGION_SCREEN,
    MC_REGION_MONITOR,
    MC_REGION_WINDOW,
    MC_REGION_RECT,
} McRegion;

typedef enum {
    MC_FORMAT_PNG = 0,
    MC_FORMAT_JPEG,
    MC_FORMAT_BMP,
} McFormat;

typedef struct {
    McCommand command;
    McRegion  region;
    int       monitor;
    McRect    rect;
    bool      has_output;
    wchar_t   output[MC_PATH_CAP];
    McFormat  format;
    bool      save;
    bool      clipboard;
    bool      cursor;
    int       delay_ms;
} McOptions;

int  mc_parse_args(int argc, wchar_t **argv, McOptions *opt,
                   wchar_t *error, size_t error_cap);
bool mc_format_from_path(const wchar_t *path, McFormat *out);
bool mc_parse_delay_ms(const wchar_t *text, int *out);
bool mc_default_filename(int year, int month, int day,
                         int hour, int minute, int second, int attempt,
                         wchar_t *out, size_t cap);

#endif
