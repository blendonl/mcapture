#include "mcapture.h"

McRect mc_rect_from_win(RECT r) {
    McRect m = { r.left, r.top, r.right - r.left, r.bottom - r.top };
    return m;
}

McRect mc_virtual_screen(void) {
    McRect r = {
        GetSystemMetrics(SM_XVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN),
        GetSystemMetrics(SM_CXVIRTUALSCREEN),
        GetSystemMetrics(SM_CYVIRTUALSCREEN),
    };
    return r;
}

typedef struct {
    McMonitor *items;
    int        count;
    int        cap;
} MonitorScan;

static BOOL CALLBACK monitor_proc(HMONITOR monitor, HDC dc, LPRECT area, LPARAM param) {
    MonitorScan *scan = (MonitorScan *)param;
    if (scan->count >= scan->cap) return FALSE;

    MONITORINFOEXW info;
    memset(&info, 0, sizeof info);
    info.cbSize = sizeof info;
    if (!GetMonitorInfoW(monitor, (MONITORINFO *)&info)) return TRUE;

    McMonitor *m = &scan->items[scan->count++];
    m->rect    = mc_rect_from_win(info.rcMonitor);
    m->primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    wcsncpy(m->device, info.szDevice, CCHDEVICENAME - 1);
    m->device[CCHDEVICENAME - 1] = L'\0';
    return TRUE;
}

int mc_monitor_list(McMonitor *out, int cap) {
    McMonitor   found[MC_MAX_MONITORS];
    MonitorScan scan = { found, 0, cap < MC_MAX_MONITORS ? cap : MC_MAX_MONITORS };

    EnumDisplayMonitors(NULL, NULL, monitor_proc, (LPARAM)&scan);

    McRect rects[MC_MAX_MONITORS];
    int    order[MC_MAX_MONITORS];
    for (int i = 0; i < scan.count; i++) rects[i] = found[i].rect;

    mc_sort_by_position(rects, order, scan.count);
    for (int i = 0; i < scan.count; i++) out[i] = found[order[i]];
    return scan.count;
}

bool mc_cursor_pos(int *x, int *y) {
    POINT pt;
    if (!GetCursorPos(&pt)) return false;
    *x = pt.x;
    *y = pt.y;
    return true;
}

bool mc_window_frame(HWND hwnd, McRect *out) {
    RECT r;
    if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof r)) ||
        r.right <= r.left || r.bottom <= r.top) {
        if (!GetWindowRect(hwnd, &r)) return false;
    }

    *out = mc_rect_from_win(r);
    return !mc_rect_empty(*out);
}

bool mc_image_create(int w, int h, McImage *out) {
    memset(out, 0, sizeof *out);
    if (w <= 0 || h <= 0) return false;

    BITMAPINFO bi;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void   *bits = NULL;
    HBITMAP dib  = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!dib || !bits) {
        if (dib) DeleteObject(dib);
        return false;
    }

    out->dib  = dib;
    out->bits = (BYTE *)bits;
    out->w    = w;
    out->h    = h;
    return true;
}

void mc_image_free(McImage *img) {
    if (!img) return;
    if (img->dib) DeleteObject(img->dib);
    memset(img, 0, sizeof *img);
}

static void make_opaque(McImage *img) {
    size_t count = (size_t)img->w * (size_t)img->h;
    BYTE  *p     = img->bits;
    for (size_t i = 0; i < count; i++, p += 4) p[3] = 0xFF;
}

static void draw_cursor(HDC dc, McRect region) {
    CURSORINFO ci;
    memset(&ci, 0, sizeof ci);
    ci.cbSize = sizeof ci;
    if (!GetCursorInfo(&ci) || !(ci.flags & CURSOR_SHOWING) || !ci.hCursor) return;

    ICONINFO ii;
    if (!GetIconInfo(ci.hCursor, &ii)) return;

    int x = ci.ptScreenPos.x - (int)ii.xHotspot - region.x;
    int y = ci.ptScreenPos.y - (int)ii.yHotspot - region.y;

    if (ii.hbmMask)  DeleteObject(ii.hbmMask);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);

    DrawIconEx(dc, x, y, ci.hCursor, 0, 0, 0, NULL, DI_NORMAL);
}

bool mc_capture_screen(McRect region, bool cursor, McImage *out) {
    if (!mc_image_create(region.w, region.h, out)) {
        mc_error(L"could not allocate a %dx%d image", region.w, region.h);
        return false;
    }

    HDC     screen = GetDC(NULL);
    HDC     mem    = CreateCompatibleDC(screen);
    HBITMAP old    = (HBITMAP)SelectObject(mem, out->dib);

    BOOL  copied = BitBlt(mem, 0, 0, region.w, region.h, screen,
                          region.x, region.y, SRCCOPY | CAPTUREBLT);
    DWORD err    = copied ? 0 : GetLastError();

    if (copied && cursor) draw_cursor(mem, region);

    SelectObject(mem, old);
    DeleteDC(mem);
    ReleaseDC(NULL, screen);
    GdiFlush();

    if (!copied) {
        mc_error(L"could not copy the screen at %d,%d,%d,%d (error %lu)",
                 region.x, region.y, region.w, region.h, err);
        mc_image_free(out);
        return false;
    }

    make_opaque(out);
    return true;
}

bool mc_image_crop(const McImage *src, McRect area, McImage *out) {
    McRect bounds = { 0, 0, src->w, src->h };
    if (!mc_rect_contains_rect(bounds, area)) return false;
    if (!mc_image_create(area.w, area.h, out)) return false;

    size_t src_stride = (size_t)src->w * 4;
    size_t dst_stride = (size_t)area.w * 4;

    for (int y = 0; y < area.h; y++) {
        const BYTE *from = src->bits + (size_t)(area.y + y) * src_stride + (size_t)area.x * 4;
        memcpy(out->bits + (size_t)y * dst_stride, from, dst_stride);
    }
    return true;
}
