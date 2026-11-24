// layer.cpp - two-layer demo: a rotating / zooming BG under a FG with HUD, 50% dithered glass,
// transparent text and bushes. Arrows: FG, WASD: BG, +/-: zoom, 1/2: rotate, SPACE: auto, H: FG on/off
#include "assets.h"

// Extra tiles (the kit uses 128..134, the transparent font 32..127)
enum {
    T_EMPTY = 0,          // all pixels 0: fully transparent
    T_PANEL = 135,        // solid HUD panel
    T_FONT_OPQ = 136,     // 136..231: opaque font (background = color 1)
    T_PANEL_EDGE = 232,   // bottom edge of the panel
    T_GLASS,              // 50% dithering: fake semi-transparent glass
    T_BUSH_TOP, T_BUSH,   // bush outline (top part transparent) / solid bush
    T_FRAME_H, T_FRAME_V,
};
enum { B_RAINBOW = 0, B_WORLD, B_DUNGEON, B_HUD, B_HUD_HI };   // palette banks

// ---- Drawing on a map ---------------------------------------------------------------
// HUD and overlay are designed on 32x32 cells and repeated in the 4 quadrants of the 64x64 map
static void put(Layer& L, int x, int y, int tile, int bank, int flip = 0) {
    for (int q = 0; q < 4; ++q) set_cell(L, (x & 31) + (q & 1) * 32, (y & 31) + (q >> 1) * 32, tile, bank, flip);
}
static void fill(Layer& L, int x0, int y0, int x1, int y1, int tile, int bank, int flip = 0) {
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) put(L, x, y, tile, bank, flip);
}
// Rainbow font on a transparent background, or HUD font on a solid one
static void text(Layer& L, int x, int y, const char* s, int bank, bool opaque) {
    for (; *s; ++s) {
        int c = (uint8_t)*s;
        if (c < 32 || c > 127) c = '?';
        put(L, x++, y, opaque ? T_FONT_OPQ + c - 32 : c, bank);
    }
}
static void hud(Layer& L, int x, int y, const char* s, int bank = B_HUD) { text(L, x, y, s, bank, true); }

// ---- Tiles and palettes, identical in both layers -----------------------------------------
static void load_shared(Layer& L) {
    set_pal(L.col, B_RAINBOW, PAL_RAINBOW);
    set_pal(L.col, B_WORLD, PAL_WORLD);
    set_pal(L.col, B_DUNGEON, PAL_DUNGEON);
    set_pal(L.col, B_HUD,    "000 012 353 773 000 030 150 371 120 231");   // white text
    set_pal(L.col, B_HUD_HI, "000 300 730 760 000 030 150 371 120 231");   // yellow on dark red

    load_kit_tiles(L.tile);
    load_font(L.tile, 32, [](bool on, int row) { return on ? row + 1 : 0; });
    load_font(L.tile, T_FONT_OPQ, [](bool on, int) { return on ? 3 : 1; });

    auto tile = [&](int id, auto f) { put_tile(L.tile, id, make_tile(f)); };
    tile(T_PANEL,      [](int, int)     { return 1; });
    tile(T_PANEL_EDGE, [](int, int y)   { return y < 5 ? 1 : y == 5 ? 2 : 0; });
    tile(T_GLASS,      [](int x, int y) { return ((x + y) & 1) ? 4 : 0; });
    tile(T_FRAME_H,    [](int, int y)   { return (y == 3 || y == 4) ? 2 : 0; });
    tile(T_FRAME_V,    [](int x, int)   { return (x == 3 || x == 4) ? 2 : 0; });
    tile(T_BUSH_TOP,   [](int x, int y) {
        static const int edge[8] = {6, 4, 3, 2, 2, 3, 4, 6};   // edge height per column
        if (y < edge[x]) return 0;
        if (y == edge[x]) return 7;                            // highlight on the edge
        return (x * 3 + y) % 5 == 0 ? 7 : ((x + y) & 1) ? 6 : 5;
    });
    tile(T_BUSH,       [](int x, int y) { return (x * 5 + y * 3) % 7 == 0 ? 7 : ((x ^ y) & 2) ? 5 : 6; });
}

// Darken a layer's palette (the mixer has separate col_rams, so the other layer is not affected)
static void darken_palette(Layer& L, int percent) {
    for (int i = 0; i < 256; ++i) {
        int c = L.col[i];
        L.col[i] = rgb332((c >> 5) * percent / 100, ((c >> 2) & 7) * percent / 100, (c & 3) * percent / 100);
    }
}

// ---- BG: 64x64 plane with a central island in a lake and a pond in every quadrant --------------
static void build_bg(Layer& L) {
    const int cx = MAP_W / 2, cy = MAP_H / 2;
    static const int PONDS[4][2] = {{14, 14}, {50, 14}, {14, 50}, {50, 50}};
    auto dist2 = [](int dx, int dy) { return dx * dx + dy * dy; };

    for (int y = 0; y < MAP_H; ++y)
        for (int x = 0; x < MAP_W; ++x) {
            int d2 = dist2(x - cx, y - cy), pd2 = 1 << 30;   // distance^2 from center / nearest pond
            for (auto& p : PONDS) pd2 = std::min(pd2, dist2(x - p[0], y - p[1]));
            bool border = x == 0 || y == 0 || x == MAP_W - 1 || y == MAP_H - 1;
            bool land = d2 < 64 || (d2 >= 196 && pd2 < 5), water = d2 < 196 || pd2 < 20;
            if (border) set_cell(L, x, y, T_BRICK, B_DUNGEON);
            else if (land) set_cell(L, x, y, T_GRASS, B_WORLD, (x * 3 + y) & 3);
            else if (water) set_cell(L, x, y, T_WATER, B_WORLD, (x + y) & 1);
            else set_cell(L, x, y, (x % 5 == 2 && y % 5 == 2) ? T_COIN : T_CHECKER, B_DUNGEON);
        }

    auto tree = [&](int x, int y) {
        for (int k = 0; k < 4; ++k)
            set_cell(L, x + (k & 1), y + (k >> 1), (k >> 1) ? T_TRUNK : T_TREE, B_WORLD, k & 1);
    };
    tree(cx - 1, cy - 1);
    for (auto& p : PONDS) tree(p[0] - 1, p[1] - 1);
}

// ---- FG: HUD + overlay. T_EMPTY (or color 0) lets the BG show through --------------------------
static void build_fg(Layer& L) {
    fill(L, 0, 0, 31, 31, T_EMPTY, 0);

    // Top panel with an edge fading into transparency
    fill(L, 0, 0, 31, 1, T_PANEL, B_HUD);
    fill(L, 0, 2, 31, 2, T_PANEL_EDGE, B_HUD);
    hud(L, 1, 0, "TWO LAYERS + TRANSPARENCY");

    // Glass window
    const int x0 = 1, y0 = 4, x1 = 13, y1 = 12;
    fill(L, x0 + 1, y0 + 1, x1 - 1, y1 - 1, T_GLASS, B_HUD);
    fill(L, x0 + 1, y0, x1 - 1, y0, T_FRAME_H, B_HUD);
    fill(L, x0 + 1, y1, x1 - 1, y1, T_FRAME_H, B_HUD);
    fill(L, x0, y0 + 1, x0, y1 - 1, T_FRAME_V, B_HUD);
    fill(L, x1, y0 + 1, x1, y1 - 1, T_FRAME_V, B_HUD);
    hud(L, x0 + 2, y0 + 2, "GLASS 50%");
    hud(L, x0 + 2, y0 + 4, "DITHER:");
    hud(L, x0 + 2, y0 + 5, "1 PIXEL");
    hud(L, x0 + 2, y0 + 6, "IN 2 = 0");

    // Transparent text
    text(L, 16, 5, "COLOR 0 IS", B_RAINBOW, false);
    text(L, 16, 6, "TRANSPARENT", B_RAINBOW, false);
    text(L, 16, 8, "THE BG SHOWS", B_RAINBOW, false);
    text(L, 16, 9, "BETWEEN LETTERS", B_RAINBOW, false);

    // Bushes with an irregular outline
    for (int x = 0; x < 32; ++x) {
        int top = 25 + ((x * 7) % 3 == 0 ? 0 : 1);
        put(L, x, top, T_BUSH_TOP, B_HUD, x & 1);
        fill(L, x, top + 1, x, 29, T_BUSH, B_HUD, (x >> 1) & 3);
    }

    // Bottom bar
    fill(L, 0, 30, 31, 31, T_PANEL, B_HUD);
    hud(L, 0, 30, "ARROWS:FG WASD:BG +/-:ZOOM");
    hud(L, 0, 31, "1/2:ROTATE SPACE:AUTO H:FG");
}

// Per-frame HUD updates: just a few writes to map_ram
static void update_hud(Layer& L, double ang, double zoom, bool paused, int frame) {
    char buf[40];
    int deg = ((int)lround(ang * 180 / PI) % 360 + 360) % 360;
    snprintf(buf, sizeof buf, "ROT:%3d ZOOM:%4.2f %-5s", deg, zoom, paused ? "PAUSE" : "AUTO");
    fill(L, 0, 1, 31, 1, T_PANEL, B_HUD);
    hud(L, 1, 1, buf);
    hud(L, 18, 12, " LIVE ", ((frame / 15) & 1) ? B_HUD_HI : B_HUD);   // blinks by changing bank only
}

// ---- Main -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    sim_init(argc, argv);
    Display display;
    if (!display.open("Verilator PPU - Two-layer demo", VGA_W, VGA_H)) return 1;

    load_shared(vram.bg);
    load_shared(vram.fg);
    build_bg(vram.bg);
    darken_palette(vram.bg, 35);   // so the FG stands out
    build_fg(vram.fg);

    double cx = MAP_PX / 2, cy = MAP_PX / 2, ang = 0, user_zoom = 1, t = 0;   // BG camera
    double fg_x = 0, fg_y = 0;                                               // FG scroll
    bool paused = false, fg_visible = true, running = true;
    for (int frame = 0; running && !sim_context->gotFinish(); ++frame) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type != SDL_KEYDOWN || ev.key.repeat) continue;
            switch (ev.key.keysym.sym) {
                case SDLK_ESCAPE: running = false; break;
                case SDLK_SPACE: paused = !paused; break;
                case SDLK_h:
                    fg_visible = !fg_visible;
                    if (fg_visible) build_fg(vram.fg);
                    else fill(vram.fg, 0, 0, 31, 31, T_EMPTY, 0);
                    break;
                default: break;
            }
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        fg_x += keys[SDL_SCANCODE_LEFT] - keys[SDL_SCANCODE_RIGHT];
        fg_y += keys[SDL_SCANCODE_UP] - keys[SDL_SCANCODE_DOWN];
        cx += 2 * (keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A]);
        cy += 2 * (keys[SDL_SCANCODE_S] - keys[SDL_SCANCODE_W]);
        if (keys[SDL_SCANCODE_EQUALS]) user_zoom *= 1.03;
        if (keys[SDL_SCANCODE_MINUS]) user_zoom /= 1.03;
        ang += 0.03 * (keys[SDL_SCANCODE_2] - keys[SDL_SCANCODE_1]);
        if (!paused) { t += 1.0 / 60; ang += 0.01; }
        double zoom = user_zoom * (1 + 0.35 * sin(t * 0.8));

        if (fg_visible) update_hud(vram.fg, ang, zoom, paused, frame);
        run_frame(regs_rotzoom(0, cx, cy, -ang, zoom), regs_ident(fg_x, fg_y));
        display.present_vga();
    }

    display.close();
    sim_done();
    return 0;
}
