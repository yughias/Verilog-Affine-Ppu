// inspector.h - shows at runtime what every layer is doing, around the PPU output
#pragma once
#include "scenes.h"

// -----------------------------------------------------------------------------
// Data for the panels
// -----------------------------------------------------------------------------
// Color index (bank << 4 | color) of a layer at a 16.16 map position
static inline uint8_t layer_idx(const Layer& L, uint32_t x, uint32_t y) {
    int a = cell_index(x >> 19, y >> 19), fx = (x >> 16) & 7, fy = (y >> 16) & 7;
    uint8_t at = L.attr[a];
    if (at & 1) fx ^= 7;
    if (at & 2) fy ^= 7;
    return (uint8_t)((at & 0xF0) | tile_pixel(&L.tile[L.map[a] * TILE_BYTES], fx, fy));
}

// Sprites of each row, from the OAM the hardware scanned for it
struct RowSpr {
    int n_valid, n_loaded, scan_cycles;
    int slot_oam[8];
    int8_t who[PPU_W];          // OAM entry visible at each x, -1 = none
};
static RowSpr row_sprites[PPU_H];
static int dropped_sprites = 0;       // sprites lost to the per-line limit in the whole frame

// Same selection as sprite_layer.v: the first 8 entries on the row, in OAM order
static void scan_sprites() {
    dropped_sprites = 0;
    for (int row = 0; row < PPU_H; ++row) {
        RowSpr& R = row_sprites[row];
        const uint32_t* snap = scanned_oam[row];
        R.n_valid = 0;
        for (int i = 0; i < 32; ++i) if (((row - (int)((snap[i] >> 9) & 511)) & 511) < 8) ++R.n_valid;
        int sc = 0, cyc = 1;
        for (int oc = 0; oc < NUM_OAM && sc < SPR_LINE_LIMIT; ++oc) {
            cyc += 2;
            if (((row - (int)((snap[oc] >> 9) & 511)) & 511) >= 8) continue;
            R.slot_oam[sc++] = oc;
            cyc += 9;
        }
        R.n_loaded = sc;
        R.scan_cycles = cyc;
        dropped_sprites += std::max(0, R.n_valid - R.n_loaded);
        for (int p = 0; p < PPU_W; ++p) {
            R.who[p] = -1;
            for (int s = 0; s < sc; ++s) {
                uint32_t e = snap[R.slot_oam[s]];
                int X = e & 511, k = p + SPR_X_OFS - X;
                if (k < 0 || k > 7) continue;
                int srow = (row - (int)((e >> 9) & 511)) & 511;
                if (e & (1u << 29)) srow ^= 7;
                if (e & (1u << 28)) k ^= 7;
                if (!tile_pixel(&vram.spr_tile[((e >> 18) & 63) * TILE_BYTES], k, srow)) continue;
                R.who[p] = (int8_t)R.slot_oam[s];
                break;
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Panels
// -----------------------------------------------------------------------------
// ---- layout ----
constexpr int COMP_X = 12, COMP_Y = 52;
constexpr int GRID_X[2] = {672, 940}, GRID_Y[2] = {52, 330};
constexpr int INFO_X = 1216, INFO_Y = 52, INFO_W = 372;
constexpr int BOT_Y = 612;

// Canvas position -> PPU pixel (composite view or one of the four layer views)
static bool view_hit(bool full_view, int x, int y, int& ox, int& oy) {
    auto inside = [&](int px, int py, int s) {
        ox = (x - px) / s;
        oy = (y - py) / s;
        return x >= px && y >= py && x < px + PPU_W * s && y < py + PPU_H * s;
    };
    if (inside(COMP_X, COMP_Y, full_view ? 1 : 2)) return true;
    for (int k = 0; k < 4; ++k)
        if (inside(GRID_X[k & 1], GRID_Y[k >> 1], 1)) return true;
    return false;
}

static void badge(int x, int y, const char* s, uint32_t c) {
    fill_rect(x, y - 1, (int)strlen(s) * 8 + 4, 10, c);
    draw_text(x + 2, y, s, col::BG);
}
static void swatch(int x, int y, int w, int h, uint32_t c) {
    fill_rect(x, y, w, h, c);
    draw_rect(x, y, w, h, col::BORDER);
}
static inline double from_fixed(uint32_t v) { return (int32_t)v / 65536.0; }

// Who won the mixer: L_BG, L_FG, L_SPR or 3 = FG over a low-priority sprite
static const uint32_t WIN_COLOR[4] = {0xFF1F4E79, 0xFFB0306A, 0xFF2EA043, 0xFFE3B341};
static int win_kind(int i) {
    int w = mixer_winner(captured.fg[i], captured.sprite[i], captured.low_prio[i]);
    return (w == L_FG && (captured.sprite[i] & 15)) ? 3 : w;
}

static void cross(int ox, int oy, int s, int sx, int sy, int n, uint32_t c) {
    int cx = ox + sx * s + s / 2, cy = oy + sy * s + s / 2;
    auto blend = [&](int x, int y) {
        put_pixel(x, y, mix_color(canvas[std::min(canvas_h - 1, y) * canvas_w + std::min(canvas_w - 1, x)], c, 110));
    };
    for (int i = 0; i < n; i += 2) {
        if (abs(ox + i - cx) > 5) blend(ox + i, cy);
        if (abs(oy + i - cy) > 5) blend(cx, oy + i);
    }
    draw_rect(cx - 4 - s / 2, cy - 4 - s / 2, 9 + s, 9 + s, c);
}

static void draw_views(bool full_view, int sx, int sy) {
    // Composite: real VGA pins
    if (full_view) {
        for (int y = 0; y < VGA_H; ++y)
            for (int x = 0; x < VGA_W; ++x) put_pixel(COMP_X + x, COMP_Y + y, vga_image[y * VGA_W + x]);
        draw_rect(COMP_X - 1, COMP_Y - 1, PPU_W + 2, PPU_H + 2, mix_color(col::YELLOW, col::BG, 120));
        draw_text(COMP_X + 264, COMP_Y + 4, "<- PPU 256x256, the rest of 640x480 is blanked", col::DIM);
        cross(COMP_X, COMP_Y, 1, sx, sy, PPU_W, col::YELLOW);
    } else {
        for (int y = 0; y < PPU_H; ++y)
            for (int x = 0; x < PPU_W; ++x) fill_rect(COMP_X + x * 2, COMP_Y + y * 2, 2, 2, vga_image[y * VGA_W + x]);
        cross(COMP_X, COMP_Y, 2, sx, sy, 512, col::YELLOW);
    }

    for (int y = 0; y < PPU_H; ++y)
        for (int x = 0; x < PPU_W; ++x) {
            int i = y * PPU_W + x;
            uint8_t b = captured.bg[i], f = captured.fg[i], s = captured.sprite[i];
            bool f_tr = (f & 15) == 0, s_tr = (s & 15) == 0, lo = captured.low_prio[i];
            // 1) BG
            put_pixel(GRID_X[0] + x, GRID_Y[0] + y, to_argb(vram.bg.col[b]));
            // 2) FG (checker = transparent)
            put_pixel(GRID_X[1] + x, GRID_Y[0] + y, f_tr ? checker(x, y) : to_argb(vram.fg.col[f]));
            // 3) sprites (checker = transparent, lo-prio darker + dotted)
            uint32_t sc = checker(x, y);
            if (!s_tr) {
                sc = to_argb(vram.spr_col[s]);
                if (lo) { sc = mix_color(sc, 0xFF000000, 90); if (((x + y) & 3) == 0) sc = col::YELLOW; }
            }
            put_pixel(GRID_X[0] + x, GRID_Y[1] + y, sc);
            // 4) who won the mixer
            put_pixel(GRID_X[1] + x, GRID_Y[1] + y, mix_color(WIN_COLOR[win_kind(i)], vga_image[y * VGA_W + x], 80));
        }
    for (int gy = 0; gy < 2; ++gy)
        for (int gx = 0; gx < 2; ++gx) cross(GRID_X[gx], GRID_Y[gy], 1, sx, sy, PPU_W, col::YELLOW);
}

// Whole 512x512 map at half scale (1 panel pixel = 2x2 map pixels) + screen footprint
static void draw_mapspace(int ox, int oy, const Layer& L, bool fg, const std::vector<uint32_t>& X,
                          const std::vector<uint32_t>& Y, uint32_t accent, int sx, int sy) {
    for (int v = 0; v < 256; ++v)
        for (int u = 0; u < 256; ++u) {
            uint8_t idx = layer_idx(L, (uint32_t)(u * 2) << 16, (uint32_t)(v * 2) << 16);
            uint32_t c = (fg && (idx & 15) == 0) ? checker(u, v) : to_argb(L.col[idx]);
            put_pixel(ox + u, oy + v, mix_color(c, 0xFF000000, 90));
        }
    // footprint of the screen: every 8th line in full, plus the first/last pixel of every line
    uint32_t dim = mix_color(accent, 0xFF000000, 110);
    for (int y = 0; y < PPU_H; ++y) {
        bool full = (y & 7) == 0;
        for (int x = 0; x < PPU_W; x += full ? 1 : 255) {
            int i = y * PPU_W + x;
            put_pixel(ox + ((X[i] >> 17) & 255), oy + ((Y[i] >> 17) & 255), full ? dim : accent);
        }
    }
    int i = sy * PPU_W + sx;
    int u = (X[i] >> 17) & 255, v = (Y[i] >> 17) & 255;
    draw_rect(ox + u - 3, oy + v - 3, 7, 7, col::YELLOW);
}

// ---- Panel of the selected pixel. Every part draws at y and moves it down. ----
struct Texel { int tile = -1, bank = 0, x = 0, y = 0; };   // pixel of a stored tile, tile = -1: none

static void info_heading(int y, uint32_t c, int strength, const char* text) {
    fill_rect(INFO_X, y - 2, INFO_W - 4, 12, mix_color(col::PANEL, c, strength));
    draw_text(INFO_X + 4, y, text, c);
}
static void info_title(int& y, int sx, int sy, bool locked) {
    draw_textf(INFO_X, y, col::WHITE, "PIXEL x=%3d y=%3d", sx, sy);
    if (locked) badge(INFO_X + 150, y, "LOCK", col::YELLOW);
    draw_textf(INFO_X + 200, y, col::DIM, "VGA h=%d v=%d", sx + 4, sy);
    y += 16;
}
// Map position, cell and color of a layer
static Texel info_layer(int& y, int layer, int pixel) {
    const bool is_fg = layer == L_FG;
    const Layer& L = is_fg ? vram.fg : vram.bg;
    const uint32_t map_x = (is_fg ? fg_pos_x : bg_pos_x)[pixel], map_y = (is_fg ? fg_pos_y : bg_pos_y)[pixel];
    const uint8_t index = (is_fg ? captured.fg : captured.bg)[pixel];
    const int cell = cell_index(map_x >> 19, map_y >> 19);
    const uint8_t tile = L.map[cell], attr = L.attr[cell];

    char text[64];
    snprintf(text, sizeof text, "%s  X %+9.3f  Y %+9.3f", is_fg ? "FG" : "BG", from_fixed(map_x), from_fixed(map_y));
    info_heading(y, is_fg ? col::PINK : col::CYAN, 60, text);
    y += 13;
    draw_textf(INFO_X, y, col::TEXT, "TILE(%2d,%2d) MAP %02X ATTR %02X B%-2d F%d%d", cell % MAP_W, cell / MAP_W,
               tile, attr, attr >> 4, attr & 1, (attr >> 1) & 1);
    y += 11;
    draw_textf(INFO_X, y, col::TEXT, "IDX %02X  RGB332 %02X  %s", index, L.col[index],
               is_fg && (index & 15) == 0 ? "TRANSPARENT" : "");
    swatch(INFO_X + INFO_W - 44, y - 2, 40, 10, to_argb(L.col[index]));
    y += 15;

    Texel t;
    t.tile = tile;
    t.bank = attr >> 4;
    t.x = ((map_x >> 16) & 7) ^ ((attr & 1) ? 7 : 0);
    t.y = ((map_y >> 16) & 7) ^ ((attr & 2) ? 7 : 0);
    return t;
}
static void info_sprite_slots(int y, const RowSpr& row) {
    int x = draw_text(INFO_X, y, "SLOTS", col::DIM) + 8;
    for (int s = 0; s < 8; ++s) {
        const bool loaded = s < row.n_loaded;
        char text[16] = "--";
        if (loaded) snprintf(text, sizeof text, "%02d", row.slot_oam[s]);
        x = draw_text(x, y, text, loaded ? col::TEXT : col::DIM) + 8;
    }
}
// OAM entry that covers the pixel
static Texel info_oam_entry(int& y, int sx, int sy, int entry) {
    const RowSpr& row = row_sprites[sy];
    const uint32_t e = scanned_oam[sy][entry];
    const int oam_x = e & 511, oam_y = (e >> 9) & 511;
    const bool flip_x = (e >> 28) & 1, flip_y = (e >> 29) & 1, low_prio = (e >> 30) & 1;
    int slot = 0;
    for (int s = 0; s < row.n_loaded; ++s)
        if (row.slot_oam[s] == entry) slot = s;
    Texel t;
    t.tile = (e >> 18) & 63;
    t.bank = (e >> 24) & 15;
    t.x = (sx + SPR_X_OFS - oam_x) ^ (flip_x ? 7 : 0);
    t.y = ((sy - oam_y) & 511) ^ (flip_y ? 7 : 0);
    draw_textf(INFO_X, y, col::TEXT, "OAM #%02d SLOT %d X%3d(scr %d) Y%3d", entry, slot, oam_x, oam_x - SPR_X_OFS, oam_y);
    y += 11;
    draw_textf(INFO_X, y, col::TEXT, "TILE %2d PAL %2d FX%d FY%d %s", t.tile, t.bank, flip_x, flip_y,
               low_prio ? "LO-PRIO" : "HI-PRIO");
    return t;
}
static Texel info_sprites(int& y, int sx, int sy) {
    const RowSpr& row = row_sprites[sy];
    const int dropped = std::max(0, row.n_valid - row.n_loaded), entry = row.who[sx];
    char text[32];
    snprintf(text, sizeof text, "SPRITES ROW %d", sy);
    info_heading(y, col::GREEN, 60, text);
    y += 13;
    draw_textf(INFO_X, y, dropped ? col::RED : col::TEXT, "ON ROW %d  LOADED %d  DROPPED %d  SCAN %d CLK",
               row.n_valid, row.n_loaded, dropped, row.scan_cycles);
    y += 11;
    info_sprite_slots(y, row);
    y += 11;
    Texel t;
    if (entry >= 0) {
        t = info_oam_entry(y, sx, sy, entry);
    } else {
        draw_text(INFO_X, y, "NO SPRITE PIXEL HERE", col::DIM);
        y += 11;
        draw_textf(INFO_X, y, col::DIM, "HW sprite_idx %02X", captured.sprite[sy * PPU_W + sx]);
    }
    y += 16;
    return t;
}
// The three tiles as stored; the yellow square is the texel that was read
static void info_tile(int x, int y, const char* name, uint32_t c, const uint8_t* tiles, const uint8_t* colors,
                      const Texel& t, bool transparent) {
    draw_text(x, y, name, c);
    y += 11;
    if (t.tile >= 0) {
        draw_tile(x, y, 7, &tiles[t.tile * TILE_BYTES], colors, t.bank, 0, transparent);
        draw_textf(x + 62, y + 2, col::DIM, "%02X", t.tile);
    }
    draw_rect(x - 1, y - 1, 58, 58, col::BORDER);
    if (t.tile >= 0) draw_rect(x + t.x * 7 - 1, y + t.y * 7 - 1, 9, 9, col::YELLOW);
}
static void info_mixer(int& y, int pixel, uint32_t out) {
    static const char* const SOURCE[4] = {"BG", "FG", "SPRITE", "FG over lo sprite"};
    info_heading(y, col::WHITE, 30, "COLOR_MIXER");
    y += 13;
    draw_textf(INFO_X, y, col::TEXT, "S_TR %d F_TR %d LO %d -> %s", (captured.sprite[pixel] & 15) == 0,
               (captured.fg[pixel] & 15) == 0, captured.low_prio[pixel], SOURCE[win_kind(pixel)]);
    y += 12;
    swatch(INFO_X, y, 50, 20, out);
    draw_textf(INFO_X + 58, y + 2, col::WHITE, "VGA #%06X", (unsigned)(out & 0xFFFFFF));
    y += 30;
}
// Layer registers at the start of the line
static void info_registers(int y, int line) {
    const LineRegs& l = line_regs[line];
    char text[32];
    snprintf(text, sizeof text, "REGISTERS AT LINE %d", line);
    info_heading(y, col::YELLOW, 50, text);
    int x = INFO_X + 190;
    if (l.we_bg) { badge(x, y, "WE BG", col::CYAN); x += 52; }
    if (l.we_fg) { badge(x, y, "WE FG", col::PINK); x += 52; }
    if (l.oam) badge(x, y, "OAM", col::YELLOW);
    y += 13;
    for (int k = 0; k < 2; ++k) {
        draw_textf(INFO_X, y, k ? col::PINK : col::CYAN, "%s X0 %+9.3f  Y0 %+9.3f", k ? "FG" : "BG",
                   from_fixed(l.x0[k]), from_fixed(l.y0[k]));
        y += 11;
        draw_textf(INFO_X, y, col::TEXT, "   M %+7.3f %+7.3f %+7.3f %+7.3f", from_fixed(l.m[k][0]),
                   from_fixed(l.m[k][1]), from_fixed(l.m[k][2]), from_fixed(l.m[k][3]));
        y += 13;
    }
    draw_text(INFO_X, y, "M = m00 m01 m10 m11 (x/px y/px x/line y/line)", col::DIM);
}
static void draw_pixel_info(int sx, int sy, bool locked) {
    const int pixel = sy * PPU_W + sx;
    int y = INFO_Y + 4;
    info_title(y, sx, sy, locked);
    const Texel bg = info_layer(y, L_BG, pixel), fg = info_layer(y, L_FG, pixel);
    const Texel sprite = info_sprites(y, sx, sy);
    info_tile(INFO_X, y, "BG TILE", col::CYAN, vram.bg.tile, vram.bg.col, bg, false);
    info_tile(INFO_X + 124, y, "FG TILE", col::PINK, vram.fg.tile, vram.fg.col, fg, true);
    info_tile(INFO_X + 248, y, "SPR TILE", col::GREEN, vram.spr_tile, vram.spr_col, sprite, true);
    y += 77;
    info_mixer(y, pixel, vga_image[sy * VGA_W + sx]);
    info_registers(y, sy);
}

static void draw_sprite_sheet(int ox, int oy, int pal, int sel_tile) {
    for (int t = 0; t < SPR_TILES; ++t)
        draw_tile(ox + (t & 7) * 32, oy + (t >> 3) * 32, 4, &vram.spr_tile[t * TILE_BYTES], vram.spr_col, pal, 0, true);
    if (sel_tile >= 0) draw_rect(ox + (sel_tile & 7) * 32 - 1, oy + (sel_tile >> 3) * 32 - 1, 34, 34, col::YELLOW);
}

static void draw_palettes_and_stats(int ox, int oy, int sy) {
    const uint8_t* cr[3] = {vram.bg.col, vram.fg.col, vram.spr_col};
    const char* nm[3] = {"BG", "FG", "SPR"};
    const uint32_t ac[3] = {col::CYAN, col::PINK, col::GREEN};
    for (int k = 0; k < 3; ++k) {
        int x0 = ox + k * 88;
        draw_text(x0, oy, nm[k], ac[k]);
        for (int i = 0; i < 256; ++i) fill_rect(x0 + (i & 15) * 5, oy + 10 + (i >> 4) * 5, 5, 5, to_argb(cr[k][i]));
    }
    // sprites per line: green = loaded, red = dropped
    int gy = oy + 104;
    draw_text(ox, gy, "SPRITES/LINE (0..8)", col::TEXT);
    gy += 12;
    const int GH = 48;
    fill_rect(ox, gy, 256, GH, 0xFF141B26);
    for (int L = 0; L < PPU_H; ++L) {
        int nl = row_sprites[L].n_loaded, nv = row_sprites[L].n_valid;
        for (int k = 0; k < std::min(nv, 8); ++k)
            fill_rect(ox + L, gy + GH - (k + 1) * 6, 1, 5, k < nl ? col::GREEN : col::RED);
        if (nv > 8) put_pixel(ox + L, gy, col::RED);
    }
    for (int x = 0; x < 256; x += 2) put_pixel(ox + x, gy + GH - SPR_LINE_LIMIT * 6, col::DIM);   // the limit
    gy += GH + 6;
    draw_text(ox, gy, "WRITES/LINE: BG FG OAM", col::TEXT);
    gy += 12;
    for (int L = 0; L < PPU_H; ++L) {
        if (line_regs[L].we_bg) fill_rect(ox + L, gy, 1, 7, col::CYAN);
        if (line_regs[L].we_fg) fill_rect(ox + L, gy + 9, 1, 7, col::PINK);
        if (line_regs[L].oam)   fill_rect(ox + L, gy + 18, 1, 7, col::YELLOW);
    }
    for (int j = oy + 116; j < gy + 26; j += 2) put_pixel(ox + sy, j, col::WHITE);
}

static void draw_oam_table(int ox, int oy, int sy) {
    const RowSpr& R = row_sprites[sy];
    const uint32_t* snap = scanned_oam[sy];
    int bands = 0;
    for (int L = 0; L < PPU_H; ++L) bands += line_regs[L].oam;
    draw_textf(ox, oy, col::TEXT, "OAM AS SCANNED FOR ROW %d   (%d OAM writes this frame)", sy, bands);
    draw_text(ox, oy + 11, "white=on row  green=loaded  red=dropped  dim=other", col::DIM);
    for (int e = 0; e < 32; ++e) {
        int cx = ox + (e / 16) * 252, cy = oy + 26 + (e % 16) * 14;
        uint32_t v = snap[e];
        int X = v & 511, Y = (v >> 9) & 511, t = (v >> 18) & 63, p = (v >> 24) & 15;
        bool on = ((sy - Y) & 511) < 8, loaded = false;
        for (int s = 0; s < R.n_loaded; ++s) loaded = loaded || R.slot_oam[s] == e;
        uint32_t c = loaded ? col::GREEN : (on ? col::RED : (Y >= 256 && Y < 505 ? 0xFF3A4150 : col::DIM));
        draw_tile(cx, cy, 1, &vram.spr_tile[t * TILE_BYTES], vram.spr_col, p, 0, true);
        draw_textf(cx + 12, cy, c, "%02d X%3d Y%3d T%2d P%2d %c%c%c", e, X, Y, t, p, (v >> 28) & 1 ? 'X' : '.',
               (v >> 29) & 1 ? 'Y' : '.', (v >> 30) & 1 ? 'L' : '.');
    }
}

// ---- The whole inspector ----
struct InspectorInfo {
    int x, y;                  // selected pixel
    bool locked, full_view, paused;
    double sim_ms, ui_fps;
};

static void draw_header(const InspectorInfo& info) {
    const double mhz = info.sim_ms > 0 ? FRAME_CLOCKS / (info.sim_ms * 1000.0) : 0.0;
    draw_text(12, 8, "PPU TECH DEMO", col::CYAN, 2);
    draw_textf(232, 8, col::PINK, "SCENE %d/5  %s", demo.scene + 1, SCENE_NAME[demo.scene]);
    draw_textf(232, 20, col::DIM, "%s%s", demo.autoplay ? "AUTOPLAY" : "MANUAL",
               info.paused ? "  PAUSED (N = next frame)" : "");
    draw_textf(560, 8, col::TEXT, "FRAME %6ld   SIM %5.1f ms %5.2f MHz   UI %3.0f FPS",
               demo.frame, info.sim_ms, mhz, info.ui_fps);
    draw_textf(560, 20, dropped_sprites ? col::RED : col::DIM, "SPRITES SHOWN %d   DROPPED BY THE LINE LIMIT %d",
               demo.sprites_shown, dropped_sprites);
}
static void draw_layer_panels(const InspectorInfo& info) {
    panel(COMP_X, COMP_Y, 640, 512, info.full_view ? "VGA OUTPUT  whole FPGA 640x480  [V]"
                                                   : "VGA OUTPUT  PPU area 256x256 x2  [V]", col::WHITE);
    panel(GRID_X[0], GRID_Y[0], 256, 256, "BG layer (bg_idx)", col::CYAN);
    panel(GRID_X[1], GRID_Y[0], 256, 256, "FG layer (idx 0 = checker)", col::PINK);
    panel(GRID_X[0], GRID_Y[1], 256, 256, "SPRITES (lo-prio dotted)", col::GREEN);
    panel(GRID_X[1], GRID_Y[1], 256, 256, "WINNER", col::YELLOW);
    draw_views(info.full_view, info.x, info.y);

    static const char* const LEGEND[4] = {"BG", "FG", "SPR", "FG>LO"};   // title row of the winner view
    int x = GRID_X[1] + 64, y = GRID_Y[1] - 13;
    for (int k = 0; k < 4; ++k) {
        fill_rect(x, y, 8, 8, WIN_COLOR[k]);
        x = draw_text(x + 10, y, LEGEND[k], col::DIM) + 8;
    }
}
static void draw_bottom_panels(int sx, int sy) {
    panel(12, BOT_Y, 256, 256, "BG map 512x512 (1:2) + footprint", col::CYAN);
    draw_mapspace(12, BOT_Y, vram.bg, false, bg_pos_x, bg_pos_y, col::CYAN, sx, sy);
    panel(280, BOT_Y, 256, 256, "FG map 512x512 (1:2) + footprint", col::PINK);
    draw_mapspace(280, BOT_Y, vram.fg, true, fg_pos_x, fg_pos_y, col::PINK, sx, sy);

    int pal = SP_YELLOW, tile = -1;                  // of the sprite under the selected pixel
    const int entry = row_sprites[sy].who[sx];
    if (entry >= 0) {
        tile = (scanned_oam[sy][entry] >> 18) & 63;
        pal = (scanned_oam[sy][entry] >> 24) & 15;
    }
    char title[64];
    snprintf(title, sizeof title, "sprite tile_ram 64 tiles (pal %d)", pal);
    panel(548, BOT_Y, 256, 256, title, col::GREEN);
    draw_sprite_sheet(548, BOT_Y, pal, tile);
    panel(816, BOT_Y, 256, 256, "col_rams + per-line stats", col::PURPLE);
    draw_palettes_and_stats(816, BOT_Y, sy);
    panel(1084, BOT_Y, 504, 256, "OAM", col::YELLOW);
    draw_oam_table(1084, BOT_Y, sy);
}
static void draw_inspector(const InspectorInfo& info) {
    std::fill(canvas.begin(), canvas.end(), col::BG);
    draw_header(info);
    draw_layer_panels(info);
    panel(INFO_X, INFO_Y, INFO_W - 4, 534, "SELECTED PIXEL", col::ORANGE);
    draw_pixel_info(info.x, info.y, info.locked);
    draw_bottom_panels(info.x, info.y);
    draw_text(12, 876, "MOUSE select  CLICK lock  V 256/640x480  I demo view  SPACE pause  N next frame  "
                       "TAB/PGUP/1-5 scene  A autoplay  ESC quit", col::DIM);
    draw_text(12, 888, "Only registers and OAM change after load_vram(): watch WRITES/LINE and the OAM panel", col::DIM);
}
