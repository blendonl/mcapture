#include "geom.h"

#include <limits.h>
#include <stddef.h>

static long long right_of(McRect r)  { return (long long)r.x + r.w; }
static long long bottom_of(McRect r) { return (long long)r.y + r.h; }

static const wchar_t *parse_int(const wchar_t *s, int *out) {
    bool negative = false;
    if (*s == L'-' || *s == L'+') {
        negative = *s == L'-';
        s++;
    }
    if (*s < L'0' || *s > L'9') return NULL;

    long long value = 0;
    while (*s >= L'0' && *s <= L'9') {
        value = value * 10 + (*s - L'0');
        if (value > INT_MAX) return NULL;
        s++;
    }

    *out = (int)(negative ? -value : value);
    return s;
}

bool mc_rect_parse(const wchar_t *text, McRect *out) {
    if (!text || !out) return false;

    int            parts[4];
    const wchar_t *p = text;

    for (int i = 0; i < 4; i++) {
        p = parse_int(p, &parts[i]);
        if (!p) return false;
        if (i == 3) break;
        if (*p != L',') return false;
        p++;
    }

    if (*p != L'\0') return false;
    if (parts[2] <= 0 || parts[3] <= 0) return false;

    out->x = parts[0];
    out->y = parts[1];
    out->w = parts[2];
    out->h = parts[3];
    return true;
}

McRect mc_rect_from_points(int ax, int ay, int bx, int by) {
    McRect r;
    r.x = ax < bx ? ax : bx;
    r.y = ay < by ? ay : by;
    r.w = (ax < bx ? bx - ax : ax - bx) + 1;
    r.h = (ay < by ? by - ay : ay - by) + 1;
    return r;
}

McRect mc_rect_intersect(McRect a, McRect b) {
    long long left   = a.x > b.x ? a.x : b.x;
    long long top    = a.y > b.y ? a.y : b.y;
    long long right  = right_of(a)  < right_of(b)  ? right_of(a)  : right_of(b);
    long long bottom = bottom_of(a) < bottom_of(b) ? bottom_of(a) : bottom_of(b);

    McRect r = { (int)left, (int)top, 0, 0 };
    if (right > left && bottom > top) {
        r.w = (int)(right - left);
        r.h = (int)(bottom - top);
    }
    return r;
}

bool mc_rect_empty(McRect r) {
    return r.w <= 0 || r.h <= 0;
}

bool mc_rect_contains_point(McRect r, int x, int y) {
    return !mc_rect_empty(r) &&
           x >= r.x && (long long)x < right_of(r) &&
           y >= r.y && (long long)y < bottom_of(r);
}

bool mc_rect_contains_rect(McRect outer, McRect inner) {
    return !mc_rect_empty(outer) && !mc_rect_empty(inner) &&
           inner.x >= outer.x && inner.y >= outer.y &&
           right_of(inner) <= right_of(outer) &&
           bottom_of(inner) <= bottom_of(outer);
}

bool mc_rect_equal(McRect a, McRect b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

int mc_rect_index_at(const McRect *rects, int count, int x, int y) {
    if (!rects) return -1;
    for (int i = 0; i < count; i++)
        if (mc_rect_contains_point(rects[i], x, y)) return i;
    return -1;
}

static bool placed_before(McRect a, McRect b) {
    if (a.x != b.x) return a.x < b.x;
    return a.y < b.y;
}

void mc_sort_by_position(const McRect *rects, int *order, int count) {
    if (!rects || !order) return;

    for (int i = 0; i < count; i++) order[i] = i;

    for (int i = 1; i < count; i++) {
        int moving = order[i];
        int j      = i;
        while (j > 0 && placed_before(rects[moving], rects[order[j - 1]])) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = moving;
    }
}
