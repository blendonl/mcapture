#include <stdarg.h>

#include "tests.h"
#include "../src/cli.h"

#define END ((wchar_t *)0)

static wchar_t error[MC_ERROR_CAP];

static int parse(McOptions *opt, ...) {
    wchar_t *argv[32];
    int      argc = 0;

    argv[argc++] = L"mcapture";

    va_list ap;
    va_start(ap, opt);
    for (wchar_t *arg = va_arg(ap, wchar_t *); arg && argc < 32;
         arg = va_arg(ap, wchar_t *))
        argv[argc++] = arg;
    va_end(ap);

    return mc_parse_args(argc, argv, opt, error, MC_ERROR_CAP);
}

#define OK(...)    CHECK(parse(&o, __VA_ARGS__, END) == MC_EXIT_OK,             \
                         "expected success, got: %ls", error)
#define USAGE(...) CHECK(parse(&o, __VA_ARGS__, END) == MC_EXIT_USAGE,          \
                         "expected a usage error")

static void test_defaults(void) {
    McOptions o;

    CHECK(parse(&o, END) == MC_EXIT_OK, "no arguments is valid");
    CHECK(o.command == MC_COMMAND_CAPTURE, "captures by default");
    CHECK(o.region == MC_REGION_SELECT, "selects by default");
    CHECK(o.save && o.clipboard, "saves and copies by default");
    CHECK(o.format == MC_FORMAT_PNG, "png by default");
    CHECK(o.monitor == MC_MONITOR_UNDER_CURSOR, "monitor under the cursor by default");
    CHECK(o.delay_ms == 0 && !o.cursor && !o.has_output, "no delay, cursor or output");
}

static void test_regions(void) {
    McOptions o;

    OK(L"--screen");
    CHECK(o.region == MC_REGION_SCREEN, "--screen");
    OK(L"-f");
    CHECK(o.region == MC_REGION_SCREEN, "-f");

    OK(L"--window");
    CHECK(o.region == MC_REGION_WINDOW, "--window");
    OK(L"-s");
    CHECK(o.region == MC_REGION_SELECT, "-s");

    OK(L"--monitor");
    CHECK(o.region == MC_REGION_MONITOR && o.monitor == MC_MONITOR_UNDER_CURSOR,
          "--monitor alone is the monitor under the cursor");
    OK(L"--monitor", L"1");
    CHECK(o.monitor == 1, "--monitor 1, got %d", o.monitor);
    OK(L"-m", L"2", L"--cursor");
    CHECK(o.monitor == 2 && o.cursor, "-m 2 --cursor");
    OK(L"--monitor", L"-o", L"a.png");
    CHECK(o.monitor == MC_MONITOR_UNDER_CURSOR && o.has_output,
          "an option after --monitor is not its index");
    OK(L"--monitor=3");
    CHECK(o.monitor == 3, "--monitor=3");
    USAGE(L"--monitor=x");
    USAGE(L"--monitor=");

    OK(L"--rect", L"100,100,640,480", L"-o", L"r.png");
    CHECK(o.region == MC_REGION_RECT && o.rect.x == 100 && o.rect.y == 100 &&
          o.rect.w == 640 && o.rect.h == 480, "--rect values");
    OK(L"-r", L"-10,-20,30,40");
    CHECK(o.region == MC_REGION_RECT && o.rect.x == -10 && o.rect.y == -20,
          "a value may start with a minus sign");
    USAGE(L"-r=-10,-20,30,40");
    USAGE(L"--rect");
    USAGE(L"--rect", L"1,2,0,3");

    USAGE(L"--screen", L"--window");
    CHECK(wcsstr(error, L"choose one region") != NULL, "names the conflict: %ls", error);
    USAGE(L"--select", L"--rect", L"1,2,3,4");
}

static void test_output(void) {
    McOptions o;

    OK(L"--output", L"C:\\shots\\shot.JPG");
    CHECK(o.format == MC_FORMAT_JPEG && o.has_output, ".JPG is jpeg");
    CHECK(wcscmp(o.output, L"C:\\shots\\shot.JPG") == 0, "path is kept verbatim");
    OK(L"--output=x.jpeg");
    CHECK(o.format == MC_FORMAT_JPEG, ".jpeg is jpeg");
    OK(L"-o", L"a.bmp");
    CHECK(o.format == MC_FORMAT_BMP, ".bmp is bmp");
    OK(L"-o", L"a.b.png");
    CHECK(o.format == MC_FORMAT_PNG, "only the last extension counts");

    USAGE(L"-o", L"a.gif");
    USAGE(L"-o", L"noext");
    USAGE(L"-o", L"dir.d\\file");
    USAGE(L"-o", L"trailing.");
    USAGE(L"--output=");
    USAGE(L"-o");

    OK(L"--no-save");
    CHECK(!o.save && o.clipboard, "--no-save keeps the clipboard");
    OK(L"--no-clipboard");
    CHECK(o.save && !o.clipboard, "--no-clipboard keeps the file");
    USAGE(L"--no-save", L"-o", L"a.png");
    USAGE(L"--no-save", L"--no-clipboard");
}

static void test_delay(void) {
    McOptions o;

    OK(L"--delay", L"1.5");
    CHECK(o.delay_ms == 1500, "1.5s, got %d", o.delay_ms);
    OK(L"-d", L"0");
    CHECK(o.delay_ms == 0, "0s");
    OK(L"--delay=0.25");
    CHECK(o.delay_ms == 250, "0.25s, got %d", o.delay_ms);
    OK(L"--delay", L"3600");
    CHECK(o.delay_ms == 3600000, "the maximum");
    OK(L"--delay", L".5");
    CHECK(o.delay_ms == 500, ".5s, got %d", o.delay_ms);

    USAGE(L"--delay", L"3601");
    USAGE(L"--delay", L"abc");
    USAGE(L"--delay", L"-1");
    USAGE(L"--delay", L".");
    USAGE(L"--delay", L"1,5");
}

static void test_commands(void) {
    McOptions o;

    OK(L"--help", L"--bogus");
    CHECK(o.command == MC_COMMAND_HELP, "--help wins over what follows it");
    OK(L"-h");
    CHECK(o.command == MC_COMMAND_HELP, "-h");
    OK(L"-V");
    CHECK(o.command == MC_COMMAND_VERSION, "-V");
    OK(L"--version");
    CHECK(o.command == MC_COMMAND_VERSION, "--version");
    OK(L"--list");
    CHECK(o.command == MC_COMMAND_LIST, "--list");

    USAGE(L"--bogus", L"--help");
    CHECK(wcsstr(error, L"--bogus") != NULL, "names the unknown option: %ls", error);
    USAGE(L"-x");
    USAGE(L"bogus");
    CHECK(wcsstr(error, L"unexpected argument") != NULL, "positional: %ls", error);
    USAGE(L"--cursor=yes");
    USAGE(L"--");
}

static void test_filenames(void) {
    wchar_t name[64];

    CHECK(mc_default_filename(2026, 9, 14, 5, 1, 16, 1, name, 64), "valid time");
    CHECK(wcscmp(name, L"mcapture-20260914-050116.png") == 0, "got %ls", name);

    CHECK(mc_default_filename(2026, 12, 31, 23, 59, 59, 2, name, 64), "second attempt");
    CHECK(wcscmp(name, L"mcapture-20261231-235959-2.png") == 0, "got %ls", name);

    CHECK(mc_default_filename(2026, 1, 2, 3, 4, 5, 123, name, 64), "later attempt");
    CHECK(wcscmp(name, L"mcapture-20260102-030405-123.png") == 0, "got %ls", name);

    CHECK(!mc_default_filename(2026, 13, 1, 0, 0, 0, 1, name, 64), "month 13");
    CHECK(!mc_default_filename(2026, 1, 1, 24, 0, 0, 1, name, 64), "hour 24");
    CHECK(!mc_default_filename(2026, 1, 1, 0, 0, 0, 0, name, 64), "attempt 0");
    CHECK(!mc_default_filename(2026, 1, 1, 0, 0, 0, 1, name, 10), "short buffer");
}

int main(void) {
    test_defaults();
    test_regions();
    test_output();
    test_delay();
    test_commands();
    test_filenames();
    return tests_report("cli");
}
