// scenario.h - what the editor edits and saves, and how it reaches the simulated PPU
#pragma once
#include "../canvas.h"

static inline int fdiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }   // floor division
template <class T>
static inline T clampv(T v, T lo, T hi) { return std::max(lo, std::min(hi, v)); }

// -----------------------------------------------------------------------------
// Scenario: everything the editor saves
// -----------------------------------------------------------------------------
struct Sprite {
    int x = 0, y = 0;                       // screen position of the top-left pixel
    int tile = 0, pal = 1;
    bool on = false, fx = false, fy = false, lo = false;
};
struct Camera {
    double cx = 128, cy = 128;              // map point shown at the center of the screen
    double ang = 0, zoom = 1;
};
struct Scenario {
    uint8_t map[2][MAP_CELLS], attr[2][MAP_CELLS];
    uint8_t tile[2][LAYER_TILES * TILE_BYTES], spr_tile[SPR_TILES * TILE_BYTES];
    uint8_t col[3][256];                    // BG, FG, sprite col_ram
    Sprite spr[NUM_OAM];
    Camera cam[2];
    bool linked = true;
};
static Scenario scene;

static inline int tile_count(int set) { return set == L_SPR ? SPR_TILES : LAYER_TILES; }
static inline uint8_t* tile_ptr(int set, int id) {
    return (set == L_SPR ? scene.spr_tile : scene.tile[set]) + (id & (tile_count(set) - 1)) * TILE_BYTES;
}
static inline uint32_t sprite_oam(const Sprite& s) {
    return s.on ? oam_entry(s.x, s.y, s.tile, s.pal, s.fx, s.fy, s.lo) : OAM_HIDDEN;
}
static void clamp_sprite(Sprite& s) {
    s.x = clampv(s.x, -8, 247);
    s.y = clampv(s.y, -7, 255);
}

// Starter kit: palettes, font, terrain, sprite shapes, a grass BG, an empty FG and one sprite
static void new_scenario() {
    scene = Scenario{};
    for (int ram = 0; ram < 3; ++ram)       // free banks: grey ramp
        for (int i = 0; i < 256; ++i) scene.col[ram][i] = rgb332((i & 15) / 2, (i & 15) / 2, (i & 15) / 4);
    for (int L = 0; L < 2; ++L) {
        set_pal(scene.col[L], 0, PAL_RAINBOW);
        set_pal(scene.col[L], 1, PAL_WORLD);
        set_pal(scene.col[L], 2, PAL_DUNGEON);
        set_pal(scene.col[L], 3, "000 012 353 773 000 030 150 371 120 231 700 730 760 033 442 773");   // HUD
        load_font(scene.tile[L], 32, [](bool on, int row) { return on ? row + 1 : 0; });
        load_kit_tiles(scene.tile[L]);
    }
    load_kit_sprites(scene.spr_tile, scene.col[L_SPR]);
    for (int i = 0; i < MAP_CELLS; ++i) {
        scene.map[L_BG][i] = T_GRASS;
        scene.attr[L_BG][i] = make_attr(1, (i % MAP_W) * 3 + i / MAP_W);
    }
    for (Sprite& s : scene.spr) s.x = s.y = 124;
    scene.spr[0].on = true;
}

// -----------------------------------------------------------------------------
// Cameras -> layer registers: M = rotation / zoom, a = cos / zoom, b = sin / zoom
// -----------------------------------------------------------------------------
struct Xform { double sx, sy, a, b; };

static Xform cam_xform(const Camera& c) {
    double a = cos(c.ang) / c.zoom, b = sin(c.ang) / c.zoom, h = PPU_W / 2.0;
    return Xform{c.cx - (a * h + b * h), c.cy - (-b * h + a * h), a, b};
}
static Regs cam_regs(const Camera& c) {
    Xform x = cam_xform(c);
    return regs_mat(x.sx, x.sy, x.a, -x.b, x.b, x.a);
}
static void screen_to_map(int L, double px, double py, double& mx, double& my) {
    Xform x = cam_xform(scene.cam[L]);
    mx = x.sx + x.a * px + x.b * py;
    my = x.sy - x.b * px + x.a * py;
}
static void map_to_screen(int L, double mx, double my, double& px, double& py) {
    Xform x = cam_xform(scene.cam[L]);
    double dx = mx - x.sx, dy = my - x.sy, k = x.a * x.a + x.b * x.b;
    px = (x.a * dx - x.b * dy) / k;
    py = (x.b * dx + x.a * dy) / k;
}

// Copy the scenario into the Verilator memories (between frames = at the start of the vblank) and run
static void simulate() {
    Layer* lay[2] = {&vram.bg, &vram.fg};
    for (int L = 0; L < 2; ++L) {
        memcpy(lay[L]->map, scene.map[L], sizeof scene.map[L]);
        memcpy(lay[L]->attr, scene.attr[L], sizeof scene.attr[L]);
        memcpy(lay[L]->tile, scene.tile[L], sizeof scene.tile[L]);
        memcpy(lay[L]->col, scene.col[L], sizeof scene.col[L]);
    }
    memcpy(vram.spr_tile, scene.spr_tile, sizeof scene.spr_tile);
    memcpy(vram.spr_col, scene.col[L_SPR], sizeof scene.col[L_SPR]);
    for (int i = 0; i < NUM_OAM; ++i) vram.oam[i] = sprite_oam(scene.spr[i]);
    run_frame(cam_regs(scene.cam[L_BG]), cam_regs(scene.cam[L_FG]));
}
