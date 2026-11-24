// ppu_sim.h - what every simulation program shares: the Verilator model and its memories,
// register / OAM encoding, the frame loop with a simulated VGA monitor, and the SDL window.
// Each program is a single .cpp file, so everything here is static.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <SDL2/SDL.h>

#include "Vppu.h"
#include "Vppu___024root.h"
#include "verilated.h"

constexpr double PI = 3.14159265358979323846;

// ---- VGA timing (640x480 @ 60 Hz) and PPU geometry ------------------------------
constexpr int H_TOTAL = 800, V_TOTAL = 525, FRAME_CLOCKS = H_TOTAL * V_TOTAL;
constexpr int VGA_W = 640, VGA_H = 480;
constexpr int H_SYNC_BACK = 96 + 48, V_SYNC_BACK = 2 + 33;   // sync + back porch
constexpr int PPU_W = 256, PPU_H = 256;                      // area drawn by the PPU (top left)
constexpr int REG_WRITE_H = PPU_W + 8;                       // h of the register writes (hblank)
constexpr int VBLANK_WRITE_V = PPU_H + 4;                    // v of the writes for line 0

// Tilemaps: 64x64 cells = 512x512 px, wrap around (MAP_W_TILES / MAP_H_TILES in bg_layer.v)
constexpr int MAP_W_BITS = 6, MAP_W = 1 << MAP_W_BITS, MAP_H = 64;
constexpr int MAP_CELLS = MAP_W * MAP_H, MAP_PX = MAP_W * 8;
constexpr int TILE_BYTES = 32;                               // 8x8, 4bpp, low nibble = left pixel
constexpr int LAYER_TILES = 256, SPR_TILES = 64, NUM_OAM = 32;

enum { L_BG = 0, L_FG = 1, L_SPR = 2 };

// ---- Model and memories -----------------------------------------------------------
struct Layer { CData *map, *attr, *tile, *col; };
struct Vram {
    Layer bg, fg;
    CData *spr_tile, *spr_col;
    IData *oam;
};
static VerilatedContext* sim_context;
static Vppu* ppu;
static Vppu___024root* ppu_root;
static Vram vram;

#define PPU_MEM(path) (&ppu_root->ppu__DOT__##path##__DOT__memory[0])
static inline void bind_vram() {
    vram.bg = Layer{PPU_MEM(bg__DOT__map_ram), PPU_MEM(bg__DOT__attr_ram), PPU_MEM(bg__DOT__tile_ram),
                  PPU_MEM(mixer__DOT__bg_col_ram)};
    vram.fg = Layer{PPU_MEM(fg__DOT__map_ram), PPU_MEM(fg__DOT__attr_ram), PPU_MEM(fg__DOT__tile_ram),
                  PPU_MEM(mixer__DOT__fg_col_ram)};
    vram.spr_tile = PPU_MEM(u_sprite_layer__DOT__tile_ram);
    vram.spr_col = PPU_MEM(mixer__DOT__sprite_col_ram);
    vram.oam = PPU_MEM(u_sprite_layer__DOT__oam_ram);
}

// Map cell (x, y), wrapping like the hardware. attr = bank << 4 | flip_y << 1 | flip_x
static inline int cell_index(int x, int y) { return ((y & (MAP_H - 1)) << MAP_W_BITS) | (x & (MAP_W - 1)); }
static inline uint8_t make_attr(int bank, int flip = 0) { return (uint8_t)(((bank & 15) << 4) | (flip & 3)); }
static inline void set_cell(Layer& L, int x, int y, int tile, int bank, int flip = 0) {
    int i = cell_index(x, y);
    L.map[i] = (uint8_t)tile;
    L.attr[i] = make_attr(bank, flip);
}

// ---- Layer registers --------------------------------------------------------------
// Pixel (px, py) of the screen reads the map at
//   x = sx + m00*px + m10*py,   y = sy + m01*px + m11*py      (16.16 fixed point)
struct Regs { uint32_t sx, sy, m00, m01, m10, m11; };

static inline uint32_t to_fixed(double v) { return (uint32_t)(int64_t)llround(v * 65536.0); }   // mod 2^32
static inline double wrapm(double v) { v = fmod(v, (double)MAP_PX); return v < 0 ? v + MAP_PX : v; }

static inline Regs regs_mat(double sx, double sy, double m00, double m01, double m10, double m11) {
    return Regs{to_fixed(sx), to_fixed(sy), to_fixed(m00), to_fixed(m01), to_fixed(m10), to_fixed(m11)};
}
static inline Regs regs_ident(double sx, double sy) { return regs_mat(sx, sy, 1, 0, 0, 1); }
// Rotation + zoom that shows map point (cx, cy) at screen point (scx, scy); written for line L
static inline Regs regs_rotzoom(int L, double cx, double cy, double ang, double zoom, double scx = 128, double scy = 128) {
    double c = cos(ang) / zoom, s = sin(ang) / zoom, dx = -scx, dy = L - scy;
    return regs_mat(cx + c * dx - s * dy, cy + s * dx + c * dy, c, s, -s, c);
}

static inline void write_bg(const Regs& g) {
    ppu->scroll_x_bg = g.sx; ppu->scroll_y_bg = g.sy;
    ppu->m00_bg = g.m00; ppu->m01_bg = g.m01; ppu->m10_bg = g.m10; ppu->m11_bg = g.m11;
    ppu->we_scroll_bg = 1;
}
static inline void write_fg(const Regs& g) {
    ppu->scroll_x_fg = g.sx; ppu->scroll_y_fg = g.sy;
    ppu->m00_fg = g.m00; ppu->m01_fg = g.m01; ppu->m10_fg = g.m10; ppu->m11_fg = g.m11;
    ppu->we_scroll_fg = 1;
}

// ---- OAM --------------------------------------------------------------------------
constexpr int SPR_X_OFS = 8;                          // OAM X of a sprite at screen x = 0
constexpr int SPR_LINE_LIMIT = 8;                     // shifters in sprite_layer.v
constexpr uint32_t OAM_HIDDEN = (uint32_t)300 << 9;   // y = 300: never matches a line

// [8:0]=x [17:9]=y [23:18]=tile [27:24]=palette [28]=flip x [29]=flip y [30]=low priority
static inline uint32_t oam_word(int X, int Y, int tile, int pal, bool fx = false, bool fy = false, bool lo = false) {
    return (uint32_t)(X & 511) | ((uint32_t)(Y & 511) << 9) | ((uint32_t)(tile & 63) << 18) |
           ((uint32_t)(pal & 15) << 24) | ((uint32_t)fx << 28) | ((uint32_t)fy << 29) | ((uint32_t)lo << 30);
}
// Same, from the screen position of the top-left pixel; hidden when off screen
static inline uint32_t oam_entry(int sx, int sy, int tile, int pal, bool fx = false, bool fy = false, bool lo = false) {
    int X = sx + SPR_X_OFS;
    if (X < 0 || X > 255 || sy < -7 || sy > 255) return OAM_HIDDEN;
    return oam_word(X, sy, tile, pal, fx, fy, lo);
}

// ---- Clock, monitor and capture ------------------------------------------------------
static std::vector<uint32_t> vga_image(VGA_W * VGA_H, 0xFF000000u);    // what the monitor shows (ARGB)
struct Captured {                                                // color_mixer.v inputs, per pixel
    uint8_t bg[PPU_W * PPU_H], fg[PPU_W * PPU_H];                // bank << 4 | color
    uint8_t sprite[PPU_W * PPU_H], low_prio[PPU_W * PPU_H];
};
static Captured captured;

static inline int h_cnt() { return ppu_root->ppu__DOT__vga_h_cnt; }
static inline int v_cnt() { return ppu_root->ppu__DOT__vga_v_cnt; }

// One clock. The monitor locks to the HSYNC / VSYNC pins and samples the RGB pins.
static inline void clock_ppu() {
    static int x = 0, y = 0;
    static bool prev_h = true, prev_v = true;
    ppu->clk = 0; ppu->eval(); sim_context->timeInc(20);   // 25 MHz = 40 ns
    ppu->clk = 1; ppu->eval(); sim_context->timeInc(20);
    if (prev_h && !ppu->hsync) { x = -H_SYNC_BACK; ++y; }
    if (prev_v && !ppu->vsync) y = -V_SYNC_BACK;
    if (x >= 0 && x < VGA_W && y >= 0 && y < VGA_H)
        vga_image[y * VGA_W + x] = 0xFF000000u | ((uint32_t)ppu->vga_r << 16) | ((uint32_t)ppu->vga_g << 8) | ppu->vga_b;
    ++x;
    prev_h = ppu->hsync;
    prev_v = ppu->vsync;
}

// Call before the clock: the indices of pixel p are at the mixer inputs at h = p + 2
static inline void capture_indices(int h, int v) {
    if (v >= PPU_H || h < 2 || h >= PPU_W + 2) return;
    int i = v * PPU_W + h - 2;
    captured.bg[i] = ppu_root->ppu__DOT__bg_idx;
    captured.fg[i] = ppu_root->ppu__DOT__fg_idx;
    captured.sprite[i] = ppu_root->ppu__DOT__sprite_idx;
    captured.low_prio[i] = ppu_root->ppu__DOT__sprite_lo_prio;
}
// Which layer color_mixer.v shows for these indices
static inline int mixer_winner(uint8_t fg, uint8_t spr, bool lo) {
    bool f = (fg & 15) != 0, s = (spr & 15) != 0;
    return s && !(lo && f) ? L_SPR : f ? L_FG : L_BG;
}

// One frame, from the start of the vblank (v = 256, h = 0), with fixed layer registers
static inline void run_frame(const Regs& bg, const Regs& fg) {
    for (int i = 0; i < FRAME_CLOCKS; ++i) {
        const int h = h_cnt(), v = v_cnt();
        ppu->we_scroll_bg = ppu->we_scroll_fg = 0;
        if (h == REG_WRITE_H && v == VBLANK_WRITE_V) { write_bg(bg); write_fg(fg); }
        capture_indices(h, v);
        clock_ppu();
    }
}

// Create and reset the model, clear the memories, stop at the start of the first vblank
static inline void sim_init(int argc, char** argv) {
    sim_context = new VerilatedContext;
    sim_context->commandArgs(argc, argv);
    ppu = new Vppu(sim_context);
    ppu_root = ppu->rootp;
    bind_vram();
    for (Layer* L : {&vram.bg, &vram.fg}) {
        memset(L->map, 0, MAP_CELLS);
        memset(L->attr, 0, MAP_CELLS);
        memset(L->tile, 0, LAYER_TILES * TILE_BYTES);
        memset(L->col, 0, 256);
    }
    memset(vram.spr_tile, 0, SPR_TILES * TILE_BYTES);
    memset(vram.spr_col, 0, 256);
    std::fill(vram.oam, vram.oam + NUM_OAM, OAM_HIDDEN);

    ppu->rst = 1;
    for (int i = 0; i < 5; ++i) clock_ppu();
    ppu->rst = 0;
    while (h_cnt() != 0 || v_cnt() != PPU_H) clock_ppu();
    std::fill(vga_image.begin(), vga_image.end(), 0xFF000000u);
}
static inline void sim_done() {
    ppu->final();
    delete ppu;
    delete sim_context;
}

// ---- SDL window -----------------------------------------------------------------------
struct Display {
    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    SDL_Texture* tex = nullptr;
    int tw = 0, th = 0;

    // Window of w x h, shrunk if the desktop is smaller
    bool open(const char* title, int w, int h) {
        if (SDL_Init(SDL_INIT_VIDEO) == 0) {
            SDL_Rect d{0, 0, w, h};
            SDL_GetDisplayUsableBounds(0, &d);
            float k = std::min(1.0f, std::min(d.w * 0.95f / w, d.h * 0.9f / h));
            win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   (int)(w * k), (int)(h * k), SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
            if (win) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        }
        if (!ren) fprintf(stderr, "SDL error: %s\n", SDL_GetError());
        else SDL_RenderSetLogicalSize(ren, w, h);      // mouse events use these coordinates
        return ren != nullptr;
    }
    // Show a w x h image (stride in pixels); the texture follows size changes
    void present(const uint32_t* px, int w, int h, int stride, bool smooth = false) {
        if (!tex || w != tw || h != th) {
            if (tex) SDL_DestroyTexture(tex);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, smooth ? "1" : "0");
            tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
            SDL_RenderSetLogicalSize(ren, w, h);
            tw = w; th = h;
        }
        SDL_UpdateTexture(tex, nullptr, px, stride * (int)sizeof(uint32_t));
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, nullptr, nullptr);
        SDL_RenderPresent(ren);
    }
    void present_vga() { present(vga_image.data(), VGA_W, VGA_H, VGA_W); }
    void close() {
        if (tex) SDL_DestroyTexture(tex);
        if (ren) SDL_DestroyRenderer(ren);
        if (win) SDL_DestroyWindow(win);
        SDL_Quit();
    }
};
