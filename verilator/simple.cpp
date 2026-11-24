// simple.cpp - first test scene: one BG + FG world (same camera) and 32 sprites that can be
// moved and flipped from the keyboard. The status of the selected sprite goes to stdout.
#include "assets.h"

// ---- Scene ------------------------------------------------------------------------
constexpr int ISLAND_TOP = 8, ISLAND_BOTTOM = 17;                 // layout, in tile rows
constexpr int DUNGEON_TOP = 19, COIN_Y = 23, PLATFORM_Y = 25;
enum { B_RAINBOW = 0, B_WORLD = 1, B_DUNGEON = 2 };               // palette banks

// x, y are raw OAM coordinates (see SPR_X_OFS)
struct Sprite {
    int x, y, tile, pal;
    bool fx, fy, lo;                                              // lo: behind the FG layer
};
static Sprite SPR[NUM_OAM];

// 32 sprites = 8 tiles x 4 palettes in a grid of 8 per line (the hardware limit)
static Sprite home_sprite(int s) {
    return Sprite{16 + (s % 8) * 30, 48 + (s / 8) * 40, s % 8, 1 + s / 8, false, false, false};
}

static void bg_text(int x, int y, const char* s) {
    for (; *s; ++s) set_cell(vram.bg, x++, y, (uint8_t)*s, B_RAINBOW);
}

static void load_scene() {
    for (Layer* L : {&vram.bg, &vram.fg}) {
        set_pal(L->col, B_RAINBOW, PAL_RAINBOW);
        set_pal(L->col, B_WORLD, PAL_WORLD);
        set_pal(L->col, B_DUNGEON, PAL_DUNGEON);
        load_kit_tiles(L->tile);
    }
    load_font(vram.bg.tile, 32, [](bool on, int row) { return on ? row + 1 : 0; });

    // BG: text, island (grass inside a water border), dungeon floor, coins
    for (int y = 0; y < MAP_H; ++y)
        for (int x = 0; x < MAP_W; ++x) {
            bool shore = y == ISLAND_TOP || y == ISLAND_BOTTOM || x < 2 || x > MAP_W - 3;
            if (y >= DUNGEON_TOP) set_cell(vram.bg, x, y, T_CHECKER, B_DUNGEON);
            else if (y < ISLAND_TOP || y > ISLAND_BOTTOM) set_cell(vram.bg, x, y, ' ', B_RAINBOW);
            else if (shore) set_cell(vram.bg, x, y, T_WATER, B_WORLD, (x + y) & 1);
            else set_cell(vram.bg, x, y, T_GRASS, B_WORLD, (x * 3 + y) & 3);
        }
    bg_text(3, 1, "=== 4BPP PPU + PALETTE DEMO ===");
    bg_text(2, 2, "Arrows:Pan +/-:Zoom 1/2:Rotate");
    bg_text(36, 1, "--- 64x64 TILEMAP (512x512) ---");
    for (int ch = 64; ch <= 127; ++ch) set_cell(vram.bg, ch - 64, 4, ch, B_RAINBOW);
    for (int x = 8; x <= MAP_W - 9; x += 2) set_cell(vram.bg, x, COIN_Y, T_COIN, B_DUNGEON);

    // FG: transparent except trees (right half = left half flipped), brick border and platform
    static const int TREES[][2] = {{4, 10}, {10, 13}, {20, 10}, {25, 14}, {34, 12}, {41, 10}, {49, 14}, {56, 11}};
    for (auto& t : TREES)
        for (int k = 0; k < 4; ++k)
            set_cell(vram.fg, t[0] + (k & 1), t[1] + (k >> 1), (k >> 1) ? T_TRUNK : T_TREE, B_WORLD, k & 1);
    for (int y = DUNGEON_TOP; y < MAP_H; ++y)
        for (int x = 0; x < MAP_W; ++x) {
            bool border = y == DUNGEON_TOP || y == MAP_H - 1 || x == 0 || x == MAP_W - 1;
            bool platform = y == PLATFORM_Y && x >= 6 && x <= MAP_W - 7;
            if (border || platform) set_cell(vram.fg, x, y, T_BRICK, B_DUNGEON);
        }

    load_kit_sprites(vram.spr_tile, vram.spr_col);
    for (int s = 0; s < NUM_OAM; ++s) SPR[s] = home_sprite(s);
}

// ---- stdout HUD ---------------------------------------------------------------------
static void print_help() {
    puts("\n==================== CONTROLS ====================\n"
         "  TAB / Shift+TAB   select next / previous sprite\n"
         "  W A S D           move selected sprite\n"
         "  X / Y             flip selected sprite\n"
         "  P                 toggle low priority (behind FG)\n"
         "  R                 reset selected sprite\n"
         "  H                 show this help\n"
         "  Arrows            pan BG+FG\n"
         "  + / -             zoom BG+FG\n"
         "  1 / 2             rotate BG+FG\n"
         "  ESC               quit\n"
         "==================================================\n");
}
static void print_status(int sel) {
    static const char* TILE[8] = {"smiley", "heart", "star", "arrow", "ghost", "diamond", "mushroom", "letter F"};
    static const char* PAL[4] = {"yellow", "red", "blue", "green"};
    const Sprite& s = SPR[sel];
    printf("[sprite %2d] tile=%d (%-8s) pal=%d (%-6s) x=%3d y=%3d flipX=%d flipY=%d prio=%s\n",
           sel, s.tile, TILE[s.tile], s.pal, PAL[s.pal - 1], s.x, s.y, s.fx, s.fy, s.lo ? "LO" : "hi");
    fflush(stdout);
}

// ---- Main -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    sim_init(argc, argv);
    Display display;
    if (!display.open("Verilator VGA PPU Simulator (640x480)", VGA_W, VGA_H)) return 1;
    load_scene();

    double scroll_x = 0, scroll_y = 0, scale = 1, rotate = 0;
    int sel = 0;
    print_help();
    print_status(sel);

    bool running = true;
    while (running && !sim_context->gotFinish()) {
        bool changed = false;
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type != SDL_KEYDOWN || ev.key.repeat) continue;
            Sprite& s = SPR[sel];
            changed = true;
            switch (ev.key.keysym.scancode) {
                case SDL_SCANCODE_ESCAPE: running = false; break;
                case SDL_SCANCODE_TAB:
                    sel = (sel + ((SDL_GetModState() & KMOD_SHIFT) ? NUM_OAM - 1 : 1)) % NUM_OAM;
                    break;
                case SDL_SCANCODE_X: s.fx = !s.fx; break;
                case SDL_SCANCODE_Y: s.fy = !s.fy; break;
                case SDL_SCANCODE_P: s.lo = !s.lo; break;
                case SDL_SCANCODE_R: s = home_sprite(sel); break;
                case SDL_SCANCODE_H: print_help(); break;
                default: changed = false; break;
            }
        }
        if (!running) break;

        // Held keys: camera and selected sprite
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        scroll_x += (keys[SDL_SCANCODE_RIGHT] - keys[SDL_SCANCODE_LEFT]) * scale;
        scroll_y += (keys[SDL_SCANCODE_DOWN] - keys[SDL_SCANCODE_UP]) * scale;
        if (keys[SDL_SCANCODE_EQUALS]) scale *= 1.1;
        if (keys[SDL_SCANCODE_MINUS]) scale /= 1.1;
        rotate += (keys[SDL_SCANCODE_2] - keys[SDL_SCANCODE_1]) * 0.1;
        int dx = keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A], dy = keys[SDL_SCANCODE_S] - keys[SDL_SCANCODE_W];
        SPR[sel].x += dx;
        SPR[sel].y += dy;
        if (changed || dx || dy) print_status(sel);

        for (int i = 0; i < NUM_OAM; ++i) {
            const Sprite& s = SPR[i];
            vram.oam[i] = oam_word(s.x, s.y, s.tile, s.pal, s.fx, s.fy, s.lo);
        }
        double c = cos(rotate) / scale, sn = sin(rotate) / scale;
        Regs cam = regs_mat(scroll_x, scroll_y, c, -sn, sn, c);   // BG and FG stay aligned
        run_frame(cam, cam);
        display.present_vga();
    }

    display.close();
    sim_done();
    return 0;
}
