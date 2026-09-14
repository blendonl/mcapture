#include "mcapture.h"

#define OPEN_ATTEMPTS 20
#define OPEN_RETRY_MS 25

static HGLOBAL packed_dib(const McImage *img) {
    size_t stride = (size_t)img->w * 4;
    size_t pixels = stride * (size_t)img->h;

    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + pixels);
    if (!mem) return NULL;

    BITMAPINFOHEADER *header = (BITMAPINFOHEADER *)GlobalLock(mem);
    if (!header) {
        GlobalFree(mem);
        return NULL;
    }

    memset(header, 0, sizeof *header);
    header->biSize        = sizeof *header;
    header->biWidth       = img->w;
    header->biHeight      = img->h;
    header->biPlanes      = 1;
    header->biBitCount    = 32;
    header->biCompression = BI_RGB;
    header->biSizeImage   = (DWORD)pixels;

    BYTE *rows = (BYTE *)(header + 1);
    for (int y = 0; y < img->h; y++)
        memcpy(rows + (size_t)(img->h - 1 - y) * stride,
               img->bits + (size_t)y * stride, stride);

    GlobalUnlock(mem);
    return mem;
}

bool mc_clipboard_put(const McImage *img) {
    HGLOBAL mem = packed_dib(img);
    if (!mem) return false;

    bool opened = false;
    for (int attempt = 0; attempt < OPEN_ATTEMPTS; attempt++) {
        if (OpenClipboard(NULL)) {
            opened = true;
            break;
        }
        Sleep(OPEN_RETRY_MS);
    }
    if (!opened) {
        GlobalFree(mem);
        return false;
    }

    bool placed = EmptyClipboard() && SetClipboardData(CF_DIB, mem) != NULL;
    CloseClipboard();

    if (!placed) GlobalFree(mem);
    return placed;
}
