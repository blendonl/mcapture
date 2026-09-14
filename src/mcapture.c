#include "mcapture.h"

static const wchar_t HELP[] =
    L"usage: mcapture [region] [options]\n"
    L"\n"
    L"Take a screenshot, save it and put it on the clipboard.\n"
    L"\n"
    L"Region (pick one; default --select):\n"
    L"  -s, --select          drag a rectangle over a frozen screen; click to take\n"
    L"                        the window under the cursor; Esc or right-click cancels\n"
    L"  -f, --screen          the whole virtual screen, every monitor\n"
    L"  -m, --monitor [N]     monitor N from --list; without N, the one under the cursor\n"
    L"  -w, --window          the foreground window's visible frame\n"
    L"  -r, --rect X,Y,W,H    an explicit rectangle, in physical screen pixels\n"
    L"\n"
    L"Options:\n"
    L"  -o, --output PATH     .png, .jpg or .bmp; default\n"
    L"                        Pictures\\Screenshots\\mcapture-YYYYMMDD-HHMMSS.png\n"
    L"      --no-save         clipboard only\n"
    L"      --no-clipboard    file only\n"
    L"  -d, --delay SECONDS   wait before capturing (before the overlay for --select)\n"
    L"  -c, --cursor          include the mouse pointer\n"
    L"      --list            print the monitors: index, device, X,Y,W,H, primary\n"
    L"  -h, --help            show this help\n"
    L"  -V, --version         print the version\n"
    L"\n"
    L"Prints the saved path. Exit status: 0 ok, 1 failure, 2 bad usage, 3 cancelled.";

static int list_monitors(void) {
    McMonitor monitors[MC_MAX_MONITORS];
    int       count = mc_monitor_list(monitors, MC_MAX_MONITORS);

    if (count <= 0) {
        mc_error(L"no monitors found");
        return MC_EXIT_FAILURE;
    }

    for (int i = 0; i < count; i++) {
        McRect r = monitors[i].rect;
        mc_print(L"%d %ls %d,%d,%d,%d%ls", i, monitors[i].device, r.x, r.y, r.w, r.h,
                 monitors[i].primary ? L" primary" : L"");
    }
    return MC_EXIT_OK;
}

static int monitor_region(int wanted, McRect *out) {
    McMonitor monitors[MC_MAX_MONITORS];
    int       count = mc_monitor_list(monitors, MC_MAX_MONITORS);
    int       index = wanted;

    if (count <= 0) {
        mc_error(L"no monitors found");
        return MC_EXIT_FAILURE;
    }

    if (wanted == MC_MONITOR_UNDER_CURSOR) {
        McRect rects[MC_MAX_MONITORS];
        for (int i = 0; i < count; i++) rects[i] = monitors[i].rect;

        int x = 0, y = 0;
        index = mc_cursor_pos(&x, &y) ? mc_rect_index_at(rects, count, x, y) : -1;
        for (int i = 0; index < 0 && i < count; i++)
            if (monitors[i].primary) index = i;
        if (index < 0) index = 0;
    }

    if (index < 0 || index >= count) {
        mc_error(L"there is no monitor %d; --list shows the %d there are", wanted, count);
        return MC_EXIT_USAGE;
    }

    *out = monitors[index].rect;
    return MC_EXIT_OK;
}

static int resolve_region(const McOptions *opt, McRect *out) {
    McRect screen = mc_virtual_screen();
    McRect wanted = screen;

    switch (opt->region) {
    case MC_REGION_SELECT:
    case MC_REGION_SCREEN:
        break;

    case MC_REGION_MONITOR: {
        int code = monitor_region(opt->monitor, &wanted);
        if (code != MC_EXIT_OK) return code;
        break;
    }

    case MC_REGION_WINDOW: {
        HWND foreground = GetForegroundWindow();
        if (!foreground || !mc_window_frame(foreground, &wanted)) {
            mc_error(L"there is no foreground window to capture");
            return MC_EXIT_FAILURE;
        }
        break;
    }

    case MC_REGION_RECT:
        wanted = opt->rect;
        break;
    }

    McRect clipped = mc_rect_intersect(wanted, screen);
    if (mc_rect_empty(clipped)) {
        mc_error(L"%d,%d,%d,%d has no area on the screen (%d,%d,%d,%d)",
                 wanted.x, wanted.y, wanted.w, wanted.h,
                 screen.x, screen.y, screen.w, screen.h);
        return MC_EXIT_USAGE;
    }

    *out = clipped;
    return MC_EXIT_OK;
}

static int select_image(const McOptions *opt, McImage *out) {
    McRect  screen = mc_virtual_screen();
    McImage snapshot;

    if (!mc_capture_screen(screen, opt->cursor, &snapshot)) return MC_EXIT_FAILURE;

    McRect picked = { 0, 0, 0, 0 };
    int    code   = mc_select_region(&snapshot, screen, &picked);

    if (code == MC_EXIT_OK) {
        McRect area = mc_rect_intersect(picked, screen);
        area.x -= screen.x;
        area.y -= screen.y;
        if (mc_rect_empty(area) || !mc_image_crop(&snapshot, area, out)) {
            mc_error(L"could not cut %d,%d,%d,%d out of the screen",
                     picked.x, picked.y, picked.w, picked.h);
            code = MC_EXIT_FAILURE;
        }
    }

    mc_image_free(&snapshot);
    return code;
}

static int capture(const McOptions *opt) {
    if (opt->delay_ms > 0) Sleep((DWORD)opt->delay_ms);

    McImage image;
    memset(&image, 0, sizeof image);

    if (opt->region == MC_REGION_SELECT) {
        int code = select_image(opt, &image);
        if (code != MC_EXIT_OK) return code;
    } else {
        McRect region;
        int    code = resolve_region(opt, &region);
        if (code != MC_EXIT_OK) return code;
        if (!mc_capture_screen(region, opt->cursor, &image)) return MC_EXIT_FAILURE;
    }

    bool ok = true;

    if (opt->clipboard && !mc_clipboard_put(&image)) {
        mc_error(L"could not put the image on the clipboard");
        ok = false;
    }

    if (opt->save) {
        wchar_t path[MC_PATH_CAP];
        if (mc_resolve_output(opt, path, MC_PATH_CAP) &&
            mc_image_save(&image, path, opt->format))
            mc_print(L"%ls", path);
        else
            ok = false;
    }

    mc_image_free(&image);
    return ok ? MC_EXIT_OK : MC_EXIT_FAILURE;
}

static int run(int argc, wchar_t **argv) {
    McOptions opt;
    wchar_t   error[MC_ERROR_CAP];

    int code = mc_parse_args(argc, argv, &opt, error, MC_ERROR_CAP);
    if (code != MC_EXIT_OK) {
        mc_eprint(L"mcapture: %ls", error);
        mc_eprint(L"Try 'mcapture --help'.");
        return code;
    }

    switch (opt.command) {
    case MC_COMMAND_HELP:
        mc_print(L"%ls", HELP);
        return MC_EXIT_OK;

    case MC_COMMAND_VERSION:
        mc_print(L"%ls", MCAPTURE_VERSION_W);
        return MC_EXIT_OK;

    case MC_COMMAND_LIST:
        return list_monitors();

    case MC_COMMAND_CAPTURE:
        break;
    }

    return capture(&opt);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show) {
    int     argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        mc_error(L"could not read the command line");
        return MC_EXIT_FAILURE;
    }

    int code = run(argc, argv);
    LocalFree(argv);
    return code;
}
