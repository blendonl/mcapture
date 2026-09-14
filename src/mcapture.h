#pragma once

#ifndef MCAPTURE_VERSION
#define MCAPTURE_VERSION "dev"
#endif

#define MCAPTURE_WIDEN2(x) L##x
#define MCAPTURE_WIDEN(x)  MCAPTURE_WIDEN2(x)
#define MCAPTURE_VERSION_W MCAPTURE_WIDEN(MCAPTURE_VERSION)

#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef COBJMACROS
#define COBJMACROS
#endif

#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "cli.h"
#include "geom.h"

#define MC_MAX_MONITORS 32

typedef struct {
    HBITMAP dib;
    BYTE   *bits;
    int     w, h;
} McImage;

typedef struct {
    McRect  rect;
    bool    primary;
    wchar_t device[CCHDEVICENAME];
} McMonitor;

void   mc_print(const wchar_t *fmt, ...);
void   mc_eprint(const wchar_t *fmt, ...);
void   mc_error(const wchar_t *fmt, ...);

McRect mc_rect_from_win(RECT r);
McRect mc_virtual_screen(void);
int    mc_monitor_list(McMonitor *out, int cap);
bool   mc_cursor_pos(int *x, int *y);
bool   mc_window_frame(HWND hwnd, McRect *out);

bool   mc_image_create(int w, int h, McImage *out);
void   mc_image_free(McImage *img);
bool   mc_capture_screen(McRect region, bool cursor, McImage *out);
bool   mc_image_crop(const McImage *src, McRect area, McImage *out);

bool   mc_image_save(const McImage *img, const wchar_t *path, McFormat format);
bool   mc_clipboard_put(const McImage *img);
bool   mc_resolve_output(const McOptions *opt, wchar_t *out, size_t cap);

int    mc_select_region(const McImage *snapshot, McRect origin, McRect *picked);
