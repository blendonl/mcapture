#include "tests.h"
#include "../src/geom.h"

#define RECT_IS(r, X, Y, W, H)                                                \
    CHECK((r).x == (X) && (r).y == (Y) && (r).w == (W) && (r).h == (H),       \
          "expected %d,%d,%d,%d got %d,%d,%d,%d", (X), (Y), (W), (H),         \
          (r).x, (r).y, (r).w, (r).h)

static void test_parse(void) {
    McRect r = {0};

    CHECK(mc_rect_parse(L"100,200,640,480", &r), "plain rect parses");
    RECT_IS(r, 100, 200, 640, 480);

    CHECK(mc_rect_parse(L"-1920,-10,1920,1080", &r), "negative origin parses");
    RECT_IS(r, -1920, -10, 1920, 1080);

    CHECK(mc_rect_parse(L"+5,6,7,8", &r), "explicit plus parses");
    RECT_IS(r, 5, 6, 7, 8);

    CHECK(!mc_rect_parse(L"1,2,0,4", &r), "zero width is refused");
    CHECK(!mc_rect_parse(L"1,2,3,-4", &r), "negative height is refused");
    CHECK(!mc_rect_parse(L"1,2,3", &r), "three parts are refused");
    CHECK(!mc_rect_parse(L"1,2,3,4,5", &r), "five parts are refused");
    CHECK(!mc_rect_parse(L"1, 2,3,4", &r), "spaces are refused");
    CHECK(!mc_rect_parse(L"a,2,3,4", &r), "letters are refused");
    CHECK(!mc_rect_parse(L"", &r), "empty is refused");
    CHECK(!mc_rect_parse(L"1,2,99999999999,4", &r), "overflow is refused");
    CHECK(!mc_rect_parse(NULL, &r), "NULL is refused");
}

static void test_points(void) {
    McRect r = mc_rect_from_points(10, 20, 30, 60);
    RECT_IS(r, 10, 20, 21, 41);

    r = mc_rect_from_points(30, 60, 10, 20);
    RECT_IS(r, 10, 20, 21, 41);

    r = mc_rect_from_points(-5, 7, -5, 7);
    RECT_IS(r, -5, 7, 1, 1);
}

static void test_intersect(void) {
    McRect screen = { -1920, 0, 5760, 1080 };

    McRect r = mc_rect_intersect((McRect){ -2000, -50, 300, 200 }, screen);
    RECT_IS(r, -1920, 0, 220, 150);

    r = mc_rect_intersect((McRect){ 3000, 1000, 10000, 10000 }, screen);
    RECT_IS(r, 3000, 1000, 840, 80);

    r = mc_rect_intersect((McRect){ 9000, 0, 10, 10 }, screen);
    CHECK(mc_rect_empty(r), "disjoint rects intersect to nothing");

    r = mc_rect_intersect((McRect){ 0, 0, 10, 10 }, (McRect){ 10, 0, 10, 10 });
    CHECK(mc_rect_empty(r), "touching edges share no pixel");
}

static void test_contains(void) {
    McRect mon = { 3840, 0, 3840, 2160 };

    CHECK(mc_rect_contains_point(mon, 3840, 0), "top-left pixel is inside");
    CHECK(mc_rect_contains_point(mon, 7679, 2159), "bottom-right pixel is inside");
    CHECK(!mc_rect_contains_point(mon, 7680, 0), "right edge is outside");
    CHECK(!mc_rect_contains_point(mon, 3839, 10), "left neighbour is outside");

    CHECK(mc_rect_contains_rect(mon, mon), "a rect contains itself");
    CHECK(mc_rect_contains_rect((McRect){ 0, 0, 7680, 2160 }, mon),
          "the virtual screen contains a monitor");
    CHECK(!mc_rect_contains_rect(mon, (McRect){ 3800, 0, 100, 100 }),
          "a rect hanging off the edge is not contained");
    CHECK(!mc_rect_contains_rect(mon, (McRect){ 4000, 0, 0, 100 }),
          "an empty rect is not contained");

    CHECK(mc_rect_equal(mon, (McRect){ 3840, 0, 3840, 2160 }), "equal rects");
    CHECK(!mc_rect_equal(mon, (McRect){ 3840, 0, 3840, 2161 }), "unequal rects");
}

static void test_monitors(void) {
    McRect monitors[3] = {
        { 3840, 0, 3840, 2160 },
        { 0, 0, 3840, 2160 },
        { 0, -1080, 1920, 1080 },
    };
    int order[3];

    mc_sort_by_position(monitors, order, 3);
    CHECK(order[0] == 2 && order[1] == 1 && order[2] == 0,
          "sorted by left then top, got %d %d %d", order[0], order[1], order[2]);

    CHECK(mc_rect_index_at(monitors, 3, 4000, 100) == 0, "point on the right monitor");
    CHECK(mc_rect_index_at(monitors, 3, 10, -5) == 2, "point on the monitor above");
    CHECK(mc_rect_index_at(monitors, 3, 5000, 5000) == -1, "point on no monitor");

    mc_sort_by_position(monitors, order, 0);
    CHECK(1, "sorting nothing is harmless");
}

int main(void) {
    test_parse();
    test_points();
    test_intersect();
    test_contains();
    test_monitors();
    return tests_report("geom");
}
