// canvas.h - software canvas for the tools drawn around the PPU output (inspector, editor)
#pragma once

#include <cstdarg>
#include "assets.h"

static int canvas_w, canvas_h;             // canvas size
static std::vector<uint32_t> canvas;       // ARGB pixels
static inline void canvas_init(int w, int h) { canvas_w = w; canvas_h = h; canvas.assign((size_t)w * h, 0); }

namespace col {
constexpr uint32_t BG = 0xFF0A0D13, PANEL = 0xFF10151E, BORDER = 0xFF242D3D;
constexpr uint32_t TEXT = 0xFFC9D1D9, DIM = 0xFF6E7681, WHITE = 0xFFFFFFFF, BLACK = 0xFF000000;
constexpr uint32_t CYAN = 0xFF39C5E6, PINK = 0xFFFF5FA2, GREEN = 0xFF3FB950;
constexpr uint32_t RED = 0xFFF85149, YELLOW = 0xFFE3B341, ORANGE = 0xFFF0883E, PURPLE = 0xFFA371F7;
}

static inline void put_pixel(int x, int y, uint32_t c) {
    if ((unsigned)x < (unsigned)canvas_w && (unsigned)y < (unsigned)canvas_h) canvas[y * canvas_w + x] = c;
}
static inline uint32_t mix_color(uint32_t a, uint32_t b, int t) {   // t: 0..256 (0 = a)
    uint32_t out = 0xFF000000u;
    for (int s = 0; s <= 16; s += 8) out |= ((((a >> s) & 255) * (256 - t) + ((b >> s) & 255) * t) >> 8) << s;
    return out;
}
static inline void fill_rect(int x, int y, int w, int h, uint32_t c) {
    int x0 = std::max(x, 0), y0 = std::max(y, 0), x1 = std::min(x + w, canvas_w), y1 = std::min(y + h, canvas_h);
    for (int j = y0; j < y1; ++j)
        for (int i = x0; i < x1; ++i) canvas[j * canvas_w + i] = c;
}
static inline void draw_rect(int x, int y, int w, int h, uint32_t c) {
    for (int i = x; i < x + w; ++i) { put_pixel(i, y, c); put_pixel(i, y + h - 1, c); }
    for (int j = y; j < y + h; ++j) { put_pixel(x, j, c); put_pixel(x + w - 1, j, c); }
}
// Line clipped to the rectangle [cx0, cx1) x [cy0, cy1)
static inline void draw_line(double fx0, double fy0, double fx1, double fy1, uint32_t c, int cx0, int cy0, int cx1, int cy1) {
    if (std::max(std::max(fabs(fx0), fabs(fy0)), std::max(fabs(fx1), fabs(fy1))) > 20000) return;
    int x0 = (int)lround(fx0), y0 = (int)lround(fy0), x1 = (int)lround(fx1), y1 = (int)lround(fy1);
    int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        if (x0 >= cx0 && x0 < cx1 && y0 >= cy0 && y0 < cy1) put_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// ---- text (both return the x after the last character) ----
static inline int draw_text(int x, int y, const char* s, uint32_t c, int sc = 1) {
    for (; *s; ++s, x += 8 * sc) {
        int ch = (uint8_t)*s;
        if (ch < 32 || ch > 127) ch = '?';
        for (int yy = 0; yy < 8; ++yy)
            for (int xx = 0; xx < 8; ++xx)
                if (font_px(ch, xx, yy)) fill_rect(x + xx * sc, y + yy * sc, sc, sc, c);
    }
    return x;
}
static inline int draw_textf(int x, int y, uint32_t c, const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return draw_text(x, y, buf, c);
}

// ---- widgets ----
static inline void panel(int x, int y, int w, int h, const char* title, uint32_t accent) {
    fill_rect(x - 4, y - 16, w + 8, h + 20, col::PANEL);
    draw_rect(x - 4, y - 16, w + 8, h + 20, col::BORDER);
    fill_rect(x - 4, y - 16, 3, 12, accent);
    draw_text(x + 2, y - 13, title, accent);
}
static inline uint32_t checker(int x, int y) { return (((x >> 2) ^ (y >> 2)) & 1) ? 0xFF2A2F3A : 0xFF1A1E27; }

// 8x8 tile at (x, y), sc canvas pixels per tile pixel; transp0: color 0 is drawn as a checker
static inline void draw_tile(int x, int y, int sc, const uint8_t* t, const uint8_t* colr, int bank, int flip, bool transp0) {
    for (int yy = 0; yy < 8; ++yy)
        for (int xx = 0; xx < 8; ++xx) {
            int c = tile_pixel(t, (flip & 1) ? 7 - xx : xx, (flip & 2) ? 7 - yy : yy), px = x + xx * sc, py = y + yy * sc;
            fill_rect(px, py, sc, sc, (c == 0 && transp0) ? checker(px, py) : to_argb(colr[(bank & 15) * 16 + c]));
        }
}
