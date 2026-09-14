#include "mcapture.h"

#include <windowsx.h>

#define SELECT_CLASS     L"mcapture_Select"
#define SELECT_MUTEX     L"Local\\mcapture-select"
#define MAX_CANDIDATES   1024
#define DRAG_THRESHOLD   4
#define HOTKEY_CANCEL    1
#define HOTKEY_CONFIRM   2
#define ACCENT_COLOR     RGB(0, 120, 215)
#define LABEL_BACKGROUND RGB(32, 32, 32)
#define LABEL_TEXT       RGB(255, 255, 255)
#define MULTIPLICATION_SIGN 0x00D7

typedef struct {
    const McImage *snapshot;
    McImage        dimmed;
    McRect         origin;
    HWND           hwnd;
    HFONT          font;
    int            border;
    int            padding;

    McRect         windows[MAX_CANDIDATES];
    int            window_count;
    McRect         monitors[MC_MAX_MONITORS];
    int            monitor_count;

    bool           pressed;
    bool           dragging;
    POINT          anchor;
    McRect         shown;
    bool           has_shown;

    int            result;
    McRect         picked;
} SelectState;

static SelectState s;

static int scale(int px, int dpi) {
    return MulDiv(px, dpi > 0 ? dpi : 96, 96);
}

static BOOL CALLBACK collect_window(HWND hwnd, LPARAM param) {
    if (s.window_count >= MAX_CANDIDATES) return FALSE;
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;
    if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT) return TRUE;

    int cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof cloaked)) &&
        cloaked)
        return TRUE;

    McRect frame;
    if (!mc_window_frame(hwnd, &frame)) return TRUE;
    if (mc_rect_empty(mc_rect_intersect(frame, s.origin))) return TRUE;

    s.windows[s.window_count++] = frame;
    return TRUE;
}

static void collect_targets(void) {
    McMonitor monitors[MC_MAX_MONITORS];
    s.monitor_count = mc_monitor_list(monitors, MC_MAX_MONITORS);
    for (int i = 0; i < s.monitor_count; i++) s.monitors[i] = monitors[i].rect;

    s.window_count = 0;
    EnumWindows(collect_window, 0);
}

static McRect target_at(int x, int y) {
    int    index   = mc_rect_index_at(s.monitors, s.monitor_count, x, y);
    McRect monitor = index >= 0 ? s.monitors[index] : s.origin;

    for (int i = 0; i < s.window_count; i++) {
        if (!mc_rect_contains_point(s.windows[i], x, y)) continue;
        if (mc_rect_contains_rect(s.windows[i], monitor)) return monitor;

        McRect clipped = mc_rect_intersect(s.windows[i], s.origin);
        return mc_rect_empty(clipped) ? monitor : clipped;
    }
    return monitor;
}

static bool make_dimmed(void) {
    const McImage *src = s.snapshot;
    if (!mc_image_create(src->w, src->h, &s.dimmed)) return false;

    size_t      count = (size_t)src->w * (size_t)src->h;
    const BYTE *from  = src->bits;
    BYTE       *to    = s.dimmed.bits;

    for (size_t i = 0; i < count; i++, from += 4, to += 4) {
        to[0] = (BYTE)(from[0] >> 1);
        to[1] = (BYTE)(from[1] >> 1);
        to[2] = (BYTE)(from[2] >> 1);
        to[3] = 0xFF;
    }
    return true;
}

static McRect to_client(McRect r) {
    r.x -= s.origin.x;
    r.y -= s.origin.y;
    return r;
}

static void label_text(McRect sel, wchar_t *out, size_t cap) {
    _snwprintf(out, cap - 1, L"%d %lc %d", sel.w, (wint_t)MULTIPLICATION_SIGN, sel.h);
    out[cap - 1] = L'\0';
}

static SIZE label_size(HDC dc, const wchar_t *text) {
    SIZE size = { 0, 0 };
    HFONT old = (HFONT)SelectObject(dc, s.font);
    GetTextExtentPoint32W(dc, text, (int)wcslen(text), &size);
    SelectObject(dc, old);
    size.cx += s.padding * 2;
    size.cy += s.padding;
    return size;
}

static RECT label_box(McRect client_sel, SIZE size) {
    int gap = s.border + s.padding;
    int x   = client_sel.x;
    int y   = client_sel.y + client_sel.h + gap;

    if (y + size.cy > s.origin.h) y = client_sel.y - gap - size.cy;
    if (y < 0) y = client_sel.y + gap;
    if (x + size.cx > s.origin.w) x = s.origin.w - size.cx;
    if (x < 0) x = 0;

    RECT box = { x, y, x + size.cx, y + size.cy };
    return box;
}

static void invalidate_shown(void) {
    if (!s.has_shown || !s.hwnd) return;

    McRect sel = to_client(s.shown);
    RECT   area = { sel.x - s.border, sel.y - s.border,
                    sel.x + sel.w + s.border, sel.y + sel.h + s.border };
    InvalidateRect(s.hwnd, &area, FALSE);

    wchar_t text[64];
    label_text(s.shown, text, 64);

    HDC dc = GetDC(s.hwnd);
    if (!dc) return;
    RECT box = label_box(sel, label_size(dc, text));
    ReleaseDC(s.hwnd, dc);
    InvalidateRect(s.hwnd, &box, FALSE);
}

static void show(McRect r) {
    if (s.has_shown && mc_rect_equal(r, s.shown)) return;
    invalidate_shown();
    s.shown     = r;
    s.has_shown = !mc_rect_empty(r);
    invalidate_shown();
}

static void fill(HDC dc, int left, int top, int right, int bottom, HBRUSH brush) {
    RECT r = { left, top, right, bottom };
    FillRect(dc, &r, brush);
}

static void paint_decorations(HDC dc, POINT offset) {
    McRect sel = to_client(s.shown);
    int    b   = s.border;
    int    l   = sel.x - offset.x;
    int    t   = sel.y - offset.y;
    int    r   = l + sel.w;
    int    btm = t + sel.h;

    HBRUSH accent = CreateSolidBrush(ACCENT_COLOR);
    fill(dc, l - b, t - b, r + b, t, accent);
    fill(dc, l - b, btm, r + b, btm + b, accent);
    fill(dc, l - b, t, l, btm, accent);
    fill(dc, r, t, r + b, btm, accent);
    DeleteObject(accent);

    wchar_t text[64];
    label_text(s.shown, text, 64);

    SIZE size = label_size(dc, text);
    RECT box  = label_box(sel, size);
    OffsetRect(&box, -offset.x, -offset.y);

    HBRUSH background = CreateSolidBrush(LABEL_BACKGROUND);
    FillRect(dc, &box, background);
    DeleteObject(background);

    HFONT old = (HFONT)SelectObject(dc, s.font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, LABEL_TEXT);
    DrawTextW(dc, text, -1, &box, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, old);
}

static void paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC         dc   = BeginPaint(hwnd, &ps);
    RECT        area = ps.rcPaint;
    int         w    = area.right - area.left;
    int         h    = area.bottom - area.top;

    if (dc && w > 0 && h > 0) {
        HDC     buffer_dc = CreateCompatibleDC(dc);
        HBITMAP buffer    = CreateCompatibleBitmap(dc, w, h);
        HBITMAP buffer_old = (HBITMAP)SelectObject(buffer_dc, buffer);
        HDC     source_dc = CreateCompatibleDC(dc);
        HBITMAP source_old = (HBITMAP)SelectObject(source_dc, s.dimmed.dib);

        BitBlt(buffer_dc, 0, 0, w, h, source_dc, area.left, area.top, SRCCOPY);

        if (s.has_shown) {
            McRect sel     = to_client(s.shown);
            McRect dirty   = { area.left, area.top, w, h };
            McRect visible = mc_rect_intersect(sel, dirty);

            if (!mc_rect_empty(visible)) {
                SelectObject(source_dc, s.snapshot->dib);
                BitBlt(buffer_dc, visible.x - area.left, visible.y - area.top,
                       visible.w, visible.h, source_dc, visible.x, visible.y, SRCCOPY);
            }

            POINT offset = { area.left, area.top };
            paint_decorations(buffer_dc, offset);
        }

        SelectObject(source_dc, source_old);
        DeleteDC(source_dc);

        BitBlt(dc, area.left, area.top, w, h, buffer_dc, 0, 0, SRCCOPY);

        SelectObject(buffer_dc, buffer_old);
        DeleteObject(buffer);
        DeleteDC(buffer_dc);
    }

    EndPaint(hwnd, &ps);
}

static POINT screen_point(LPARAM lp) {
    POINT pt = { GET_X_LPARAM(lp) + s.origin.x, GET_Y_LPARAM(lp) + s.origin.y };
    return pt;
}

static void finish(int result, McRect picked) {
    if (s.result != -1) return;
    s.result = result;
    s.picked = picked;
    DestroyWindow(s.hwnd);
}

static void confirm_shown(void) {
    if (s.has_shown) finish(MC_EXIT_OK, s.shown);
}

static void cancel(void) {
    McRect none = { 0, 0, 0, 0 };
    finish(MC_EXIT_CANCELLED, none);
}

static void track(POINT pt) {
    if (s.pressed && !s.dragging &&
        (abs(pt.x - s.anchor.x) >= DRAG_THRESHOLD || abs(pt.y - s.anchor.y) >= DRAG_THRESHOLD))
        s.dragging = true;

    if (s.dragging) {
        McRect drag = mc_rect_from_points(s.anchor.x, s.anchor.y, pt.x, pt.y);
        show(mc_rect_intersect(drag, s.origin));
    } else {
        show(target_at(pt.x, pt.y));
    }
}

static LRESULT CALLBACK select_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT:
        paint(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_SETCURSOR:
        SetCursor(LoadCursorW(NULL, IDC_CROSS));
        return TRUE;

    case WM_MOUSEMOVE:
        track(screen_point(lp));
        return 0;

    case WM_LBUTTONDOWN:
        s.pressed  = true;
        s.dragging = false;
        s.anchor   = screen_point(lp);
        SetCapture(hwnd);
        track(s.anchor);
        return 0;

    case WM_LBUTTONUP: {
        if (!s.pressed) return 0;
        POINT pt = screen_point(lp);
        s.pressed = false;
        ReleaseCapture();
        track(pt);
        confirm_shown();
        return 0;
    }

    case WM_RBUTTONDOWN:
        return 0;

    case WM_RBUTTONUP:
        cancel();
        return 0;

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) cancel();
        else if (wp == VK_RETURN) confirm_shown();
        return 0;

    case WM_HOTKEY:
        if (wp == HOTKEY_CANCEL) cancel();
        else if (wp == HOTKEY_CONFIRM) confirm_shown();
        return 0;

    case WM_DPICHANGED:
        return 0;

    case WM_DESTROY:
        UnregisterHotKey(hwnd, HOTKEY_CANCEL);
        UnregisterHotKey(hwnd, HOTKEY_CONFIRM);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static HFONT label_font(int dpi) {
    return CreateFontW(-scale(15, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

static int screen_dpi(void) {
    HDC screen = GetDC(NULL);
    int dpi    = screen ? GetDeviceCaps(screen, LOGPIXELSX) : 96;
    if (screen) ReleaseDC(NULL, screen);
    return dpi > 0 ? dpi : 96;
}

int mc_select_region(const McImage *snapshot, McRect origin, McRect *picked) {
    HANDLE mutex = CreateMutexW(NULL, FALSE, SELECT_MUTEX);
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        mc_error(L"a selection is already open");
        return MC_EXIT_FAILURE;
    }

    memset(&s, 0, sizeof s);
    s.snapshot = snapshot;
    s.origin   = origin;
    s.result   = -1;

    int code = MC_EXIT_FAILURE;
    if (!make_dimmed()) {
        mc_error(L"could not allocate the selection overlay");
        goto out;
    }

    collect_targets();

    int dpi   = screen_dpi();
    s.border  = scale(2, dpi);
    s.padding = scale(6, dpi);
    s.font    = label_font(dpi);

    HINSTANCE   inst = GetModuleHandleW(NULL);
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof wc);
    wc.cbSize        = sizeof wc;
    wc.lpfnWndProc   = select_proc;
    wc.hInstance     = inst;
    wc.hCursor       = LoadCursorW(NULL, IDC_CROSS);
    wc.lpszClassName = SELECT_CLASS;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        mc_error(L"could not register the selection window (error %lu)", GetLastError());
        goto out;
    }

    s.hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, SELECT_CLASS, L"mcapture",
                             WS_POPUP, origin.x, origin.y, origin.w, origin.h,
                             NULL, NULL, inst, NULL);
    if (!s.hwnd) {
        mc_error(L"could not create the selection window (error %lu)", GetLastError());
        goto out;
    }

    RegisterHotKey(s.hwnd, HOTKEY_CANCEL, MOD_NOREPEAT, VK_ESCAPE);
    RegisterHotKey(s.hwnd, HOTKEY_CONFIRM, MOD_NOREPEAT, VK_RETURN);

    int cx = origin.x, cy = origin.y;
    mc_cursor_pos(&cx, &cy);
    s.shown     = target_at(cx, cy);
    s.has_shown = !mc_rect_empty(s.shown);

    ShowWindow(s.hwnd, SW_SHOW);
    UpdateWindow(s.hwnd);
    SetForegroundWindow(s.hwnd);
    SetFocus(s.hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (s.result == MC_EXIT_OK) *picked = s.picked;
    code = s.result == -1 ? MC_EXIT_CANCELLED : s.result;

out:
    if (s.font) DeleteObject(s.font);
    mc_image_free(&s.dimmed);
    UnregisterClassW(SELECT_CLASS, GetModuleHandleW(NULL));
    if (mutex) CloseHandle(mutex);
    return code;
}
