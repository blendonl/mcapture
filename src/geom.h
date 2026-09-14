#ifndef MCAPTURE_GEOM_H
#define MCAPTURE_GEOM_H

#include <stdbool.h>
#include <wchar.h>

typedef struct {
    int x, y, w, h;
} McRect;

bool   mc_rect_parse(const wchar_t *text, McRect *out);
McRect mc_rect_from_points(int ax, int ay, int bx, int by);
McRect mc_rect_intersect(McRect a, McRect b);
bool   mc_rect_empty(McRect r);
bool   mc_rect_contains_point(McRect r, int x, int y);
bool   mc_rect_contains_rect(McRect outer, McRect inner);
bool   mc_rect_equal(McRect a, McRect b);
int    mc_rect_index_at(const McRect *rects, int count, int x, int y);
void   mc_sort_by_position(const McRect *rects, int *order, int count);

#endif
