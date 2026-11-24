// graphics.h - tiles, palettes and maps of the tech demo, loaded once by load_vram()
#pragma once
#include "../canvas.h"

constexpr int NUM_SCENES = 5;

static inline void bg_cell(int x, int y, int t, int bank, int flip = 0) { set_cell(vram.bg, x, y, t, bank, flip); }
// FG: designed 32 columns wide. Every cell is written at x and x+32, so text rows,
// sky panorama and marquees stay seamless now that the X wrap is at 512 px.
static inline void fg_cell(int x, int y, int t, int bank, int flip = 0) {
    for (int qx = 0; qx < MAP_W; qx += 32) set_cell(vram.fg, (x & 31) + qx, y, t, bank, flip);
}

static uint32_t random_state = 0x1234567u;
static inline uint32_t rnd() { random_state = random_state * 1664525u + 1013904223u; return random_state >> 16; }

// -----------------------------------------------------------------------------
// VRAM layout
// -----------------------------------------------------------------------------
// BG tiles / banks
enum : int { BT_SOLID = 0, BT_GRASS_A, BT_GRASS_B, BT_ROAD, BT_DASH_H, BT_DASH_V, BT_CURB, BT_FINISH,
             BT_WATER, BT_SHORE_H, BT_SHORE_V, BT_SHORE_C, BT_SAND, BT_TREE, BT_BRICK };
enum : int { BB_GRASS = 0, BB_ROAD = 1, BB_WATER = 2, BB_BRICK_R = 3, BB_DRY = 4, BB_BRICK_B = 5, BB_BRICK_G = 6 };
// FG tiles / banks / map rows
enum : int { FT_EMPTY = 0, FT_FONT = 32 /*32..95*/, FT_OPAQUE = 96 /*96..159 = ch+64*/,
             FT_SHAFT = 160, FT_CAP = 161, FT_PANO = 170 /*170..255, deduplicated*/ };
enum : int { FB_TITLE = 0, FB_STATUS = 1, FB_RAINBOW = 2, FB_SKY = 3, FB_MARBLE = 4, FB_LABEL = 5, FB_COOL = 6 };
enum : int { FR_SKY = 0 /*0..7*/, FR_EMPTY = 8 /*8..9*/, FR_PILLARS = 10 /*10..17*/, FR_TEXT_A = 18,
             FR_TEXT_B = 19, FR_LABEL = 20 /*20..22*/, FR_STATUS = 23, FR_TITLE = 24 /*24..28*/, FR_STORM = 30 };
// Sprite tiles / palettes
enum : int { ST_BLANK = 0, ST_DIGIT0 = 1 /*1..10*/, ST_SMILEY = 11, ST_HEART, ST_STAR, ST_ARROW, ST_GHOST,
             ST_DIAMOND, ST_MUSHROOM, ST_LETTERF /*18*/, ST_KART_TL = 19, ST_KART_BL = 20, ST_COIN0 = 21,
             ST_COIN1 = 22, ST_COIN2 = 23, ST_ORB = 24, ST_SHIP = 25, ST_CONE = 26, ST_SPARK = 27 };
enum : int { SP_YELLOW = 1, SP_RED, SP_BLUE, SP_GREEN, SP_HUD, SP_GOLD, SP_KART, SP_PURPLE, SP_CYAN,
             SP_ORANGE, SP_CONE, SP_SHIP };

struct Art { int id; const char* text; };     // tile art or palette: see art() and set_pal() in assets.h
static const Art BG_ART[] = {
    {BT_GRASS_A, "00100000 01000200 00000010 20000000 00000100 01000002 00020010 00001000"},
    {BT_GRASS_B, "00000000 00300020 03430000 00300001 00000000 10000300 00203430 00000300"},
    {BT_ROAD,    "00010000 00000020 01000000 00000100 00200000 00000001 10000000 00002000"},
    {BT_DASH_H,  "00010000 00000020 01000000 03333330 03333330 00000001 10000000 00002000"},
    {BT_DASH_V,  "00010000 00033020 01033000 00033100 00233000 00033001 10033000 00002000"},
    {BT_CURB,    "44445555 44445555 44445555 44445555 55554444 55554444 55554444 55554444"},
    {BT_FINISH,  "66556655 66556655 55665566 55665566 66556655 66556655 55665566 55665566"},
    {BT_WATER,   "00001100 00110000 11000000 00000020 00000011 01100000 00011200 00000000"},
    {BT_SHORE_H, "44454444 45444454 44444444 66666666 36333363 13311331 11111111 01100110"},
    {BT_SAND,    "44444544 45444444 44444454 44544444 44444444 54444454 44445444 44444444"},
    {BT_TREE,    "00777700 07566570 75666657 75665557 75655557 75555557 07555570 08777780"},
    {BT_BRICK,   "22202222 11301113 11301113 00000000 22222022 11113011 11113011 00000000"},
};
static const Art FG_ART[] = {
    {FT_SHAFT, "71234565 71234565 71234565 71234565 71234565 71234565 71234565 71234565"},
    {FT_CAP,   "77777777 76666666 75555555 77777777 07345656 07345656 71234565 71234565"},
};
static const Art SPR_ART[] = {   // 1 outline, 2 main, 3 highlight, 4 accent, 5/6 extra
    {ST_STAR,    "...11... ..1231.. 11122111 12222221 .122221. .121121. 121..121 11....11"},
    {ST_KART_TL, "........ .....111 ....1444 ....1445 ....1455 ...11111 ..122222 .1233222"},
    {ST_KART_BL, "12322222 12222222 11111111 1661.166 1661.111 1661.... 1111.... ........"},
    {ST_COIN0,   "..1111.. .122321. 12233221 12232221 12232221 12222221 .122221. ..1111.."},
    {ST_COIN1,   "...11... ..1231.. ..1221.. ..1231.. ..1221.. ..1221.. ..1221.. ...11..."},
    {ST_COIN2,   "...11... ...31... ...21... ...21... ...21... ...21... ...21... ...11..."},
    {ST_ORB,     "..1111.. .123321. 12332221 12322221 12222241 12222441 .124441. ..1111.."},
    {ST_SHIP,    "...11... ..1321.. ..1221.. .122221. 12244221 12222221 1.1221.1 ...44..."},
    {ST_CONE,    "...11... ...21... ..1221.. ..1331.. .122221. .133331. 12222221 11111111"},
    {ST_SPARK,   "...1.... ...2.... ..131... 1233321. ..131... ...2.... ...1.... ........"},
};
static const Art BG_PAL[] = {
    {BB_GRASS,   "140 250 030 761 773 040 261 020 110"},
    {BB_DRY,     "440 551 330 730 772 330 540 220 110"},
    {BB_ROAD,    "221 331 110 773 600 773 000"},
    {BB_WATER,   "012 023 143 673 651 541 431"},
    {BB_BRICK_R, "210 610 731 400"},
    {BB_BRICK_B, "001 123 343 012"},
    {BB_BRICK_G, "010 150 371 030"},
};
static const Art FG_PAL[] = {
    {FB_TITLE,   "000 772 771 761 760 750 740 730 620 000 000 000 000 000 000 102"},
    {FB_STATUS,  "000 773 673 673 573 563 463 453 353 000 000 000 000 000 000 111"},
    {FB_RAINBOW, "000 711 730 760 271 063 143 423 733 000"},
    {FB_SKY,     "000 011 012 122 133 243 353 773 553 211 321 773"},
    {FB_MARBLE,  "000 221 331 442 552 663 773 110"},
    {FB_LABEL,   "000 773 773 663 663 662 552 552 442 000 000 000 000 000 000 021"},
    {FB_COOL,    "000 373 273 163 053 043 133 223 313 000"},
};
static const Art SPR_PAL[] = {
    {SP_YELLOW, "000 000 760 773 510"},
    {SP_RED,    "000 000 600 742 770"},
    {SP_BLUE,   "000 000 023 353 773"},
    {SP_GREEN,  "000 000 160 471 502"},
    {SP_HUD,    "000 000 773 760"},
    {SP_GOLD,   "000 210 650 772 730"},
    {SP_KART,   "000 000 600 731 773 012 331"},
    {SP_PURPLE, "000 000 413 643 773"},
    {SP_CYAN,   "000 000 053 373 773"},
    {SP_ORANGE, "000 000 730 761 410"},
    {SP_CONE,   "000 000 720 773"},
    {SP_SHIP,   "000 000 663 053 740"},
};

// Sky panorama for the FG rows 0..7 (256x64, period 128 px), deduplicated into tiles
static uint8_t pano_px(int x, int y) {
    static const int B4[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    int gi = (int)floor(y * 6.0 / 64.0 + (B4[y & 3][x & 3] + 0.5) / 16.0);
    uint8_t c = (uint8_t)(1 + std::min(gi, 5));
    int xm = x & 127;
    static const int CL[7][3] = {{18, 16, 6}, {27, 12, 8}, {37, 16, 6}, {46, 18, 4}, {80, 24, 5}, {89, 21, 7}, {99, 24, 5}};
    for (auto& b : CL) {
        int dx = xm - b[0], dy = y - b[1];
        if (dx * dx + dy * dy <= b[2] * b[2]) c = (dy * 3 > b[2]) ? 8 : 7;
    }
    double rr = 45 + 7 * sin(2 * PI * xm / 128.0) + 4 * sin(6 * PI * xm / 128.0 + 1) + 2 * sin(10 * PI * xm / 128.0 + 2);
    int ridge = std::max(32, (int)lround(rr));
    if (y >= ridge) {
        if (ridge < 40 && y < ridge + 2) c = 11;                    // snow on the peaks
        else if (y < 52 || (y == 52 && (x & 1))) c = 10;            // lit rock
        else c = 9;                                                 // dark rock
    }
    return c;
}

static int fg_pano_tile(std::vector<TilePixels>& pool, const TilePixels& p) {
    const int max_n = 256 - FT_PANO;
    for (size_t i = 0; i < pool.size(); ++i)
        if (pool[i] == p) return FT_PANO + (int)i;
    if ((int)pool.size() < max_n) {
        pool.push_back(p);
        put_tile(vram.fg.tile, FT_PANO + (int)pool.size() - 1, p);
        return FT_PANO + (int)pool.size() - 1;
    }
    int best = 0, bd = 1 << 30;                 // out of tiles: closest existing one
    for (size_t i = 0; i < pool.size(); ++i) {
        int d = 0;
        for (int k = 0; k < 64; ++k) d += pool[i][k] != p[k];
        if (d < bd) { bd = d; best = (int)i; }
    }
    return FT_PANO + best;
}

static void fg_text(int col, int row, const char* s, int bank, bool opaque) {
    for (int i = 0; s[i] && col + i < 32; ++i) {
        int c = (uint8_t)s[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 32 || c > 95) c = '?';
        fg_cell(col + i, row, opaque ? c + 64 : c, bank, 0);
    }
}
static void fg_bar(int row, const char* s, int bank) {   // 32 opaque cells
    for (int x = 0; x < 32; ++x) fg_cell(x, row, FT_OPAQUE, bank, 0);  // opaque ' '
    fg_text(0, row, s, bank, true);
}

// -----------------------------------------------------------------------------
// The 64x64 world: L-shaped circuit (clockwise), centreline in tile coordinates.
// Road = up to 2 tiles from the centreline, curb up to 3.
// -----------------------------------------------------------------------------
static const int TRACK[][2] = {{8, 8}, {55, 8}, {55, 30}, {36, 30}, {36, 55}, {8, 55}};
constexpr int TRACK_N = 6;
constexpr int FINISH_TX = 30;                       // finish line on the top straight

// Distance from tile (x,y) to the centreline; *seg = nearest segment
static double track_dist(double x, double y, int* seg) {
    double best = 1e9;
    for (int i = 0; i < TRACK_N; ++i) {
        double ax = TRACK[i][0], ay = TRACK[i][1];
        double bx = TRACK[(i + 1) % TRACK_N][0], by = TRACK[(i + 1) % TRACK_N][1];
        double dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy;
        double t = std::max(0.0, std::min(1.0, ((x - ax) * dx + (y - ay) * dy) / len2));
        double px = ax + t * dx - x, py = ay + t * dy - y, d = sqrt(px * px + py * py);
        if (d < best) { best = d; if (seg) *seg = i; }
    }
    return best;
}

// Rectangular lake with shore tiles (flips select the side / corner)
static void lake(int x0, int y0, int x1, int y1) {
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            bool top = y == y0, bot = y == y1, lft = x == x0, rgt = x == x1;
            if ((top || bot) && (lft || rgt)) bg_cell(x, y, BT_SHORE_C, BB_WATER, (rgt ? 1 : 0) | (bot ? 2 : 0));
            else if (top || bot) bg_cell(x, y, BT_SHORE_H, BB_WATER, bot ? 2 : 0);
            else if (lft || rgt) bg_cell(x, y, BT_SHORE_V, BB_WATER, rgt ? 1 : 0);
            else bg_cell(x, y, BT_WATER, BB_WATER, rnd() & 3);
        }
}

static inline bool in_ellipse(int x, int y, double cx, double cy, double rx, double ry) {
    double u = (x - cx) / rx, v = (y - cy) / ry;
    return u * u + v * v < 1.0;
}

// Grass everywhere, random tile + flip
static void draw_grass() {
    for (int y = 0; y < MAP_H; ++y)
        for (int x = 0; x < MAP_W; ++x)
            bg_cell(x, y, (rnd() % 5 == 0) ? BT_GRASS_B : BT_GRASS_A, BB_GRASS, rnd() & 3);
}

// Dry grass patches: same tile, other palette bank
static void draw_dry_grass() {
    for (int y = 0; y < MAP_H; ++y)
        for (int x = 0; x < MAP_W; ++x)
            if (in_ellipse(x, y, 21, 42, 9, 7) || in_ellipse(x, y, 46, 24, 5, 3))
                bg_cell(x, y, BT_GRASS_A, BB_DRY, rnd() & 3);
}

// Forest in the lower-left part of the L
static void draw_forest() {
    for (int y = 34; y <= 51; ++y)
        for (int x = 12; x <= 31; ++x)
            if (rnd() % 6 == 0) bg_cell(x, y, BT_TREE, BB_GRASS, rnd() & 1);
}

// "PPU" logo: one brick tile, three palette banks
static void draw_logo() {
    static const char* P[5] = {"###.", "#..#", "###.", "#...", "#..."};
    static const char* U[5] = {"#..#", "#..#", "#..#", "#..#", ".##."};
    const char** L[3] = {P, P, U};
    const int bank[3] = {BB_BRICK_R, BB_BRICK_B, BB_BRICK_G};
    for (int k = 0; k < 3; ++k)
        for (int r = 0; r < 5; ++r)
            for (int c = 0; c < 4; ++c)
                if (L[k][r][c] == '#') bg_cell(14 + k * 5 + c, 14 + r, BT_BRICK, bank[k]);
}

// Lakes: one inside the L, a big one in the outer notch with a sandy island
static void draw_lakes() {
    lake(32, 13, 45, 21);
    lake(42, 37, 58, 58);
    for (int y = 46; y <= 48; ++y)
        for (int x = 49; x <= 51; ++x) bg_cell(x, y, BT_SAND, BB_WATER, rnd() & 1);
    bg_cell(50, 47, BT_TREE, BB_GRASS);
}

// Scattered trees (flip X = variety)
static void draw_trees() {
    static const int TREES[][2] = {{13, 22}, {20, 22}, {27, 22}, {29, 13}, {48, 13}, {48, 19}, {23, 12},
                                   {50, 25}, {38, 24}, {4, 10}, {4, 22}, {4, 34}, {4, 46}, {13, 3},
                                   {25, 3}, {37, 3}, {49, 3}, {59, 12}, {59, 24}, {40, 60}, {20, 60}};
    for (auto& t : TREES) bg_cell(t[0], t[1], BT_TREE, BB_GRASS, (t[0] > 32) ? 1 : 0);
}

// The circuit (drawn over everything above)
static void draw_circuit() {
    for (int y = 0; y < MAP_H; ++y)
        for (int x = 0; x < MAP_W; ++x) {
            int seg = 0;
            double d = track_dist(x, y, &seg);
            if (d > 3.0) continue;
            if (d > 2.0) { bg_cell(x, y, BT_CURB, BB_ROAD); continue; }
            int i0 = seg, i1 = (seg + 1) % TRACK_N;
            bool horiz = TRACK[i0][1] == TRACK[i1][1];
            bool corner = false;
            for (auto& w : TRACK) if (w[0] == x && w[1] == y) corner = true;
            int t = BT_ROAD;
            if (d < 0.5 && !corner) {                           // dashed centre line
                if (horiz && !(x & 1)) t = BT_DASH_H;
                if (!horiz && !(y & 1)) t = BT_DASH_V;
            }
            if (seg == 0 && x == FINISH_TX) t = BT_FINISH;      // finish line across the top straight
            bg_cell(x, y, t, BB_ROAD);
        }
}

// Water around the world + beach
static void draw_coast() {
    for (int y = 0; y < MAP_H; ++y)
        for (int x = 0; x < MAP_W; ++x) {
            if (x <= 1 || x >= MAP_W - 2 || y <= 1 || y >= MAP_H - 2)
                bg_cell(x, y, BT_WATER, BB_WATER, rnd() & 3);
            else if (x == 2 || x == MAP_W - 3 || y == 2 || y == MAP_H - 3)
                bg_cell(x, y, BT_SAND, BB_WATER, rnd() & 1);
        }
}

// Every step draws over the previous ones
static void build_world() {
    draw_grass();
    draw_dry_grass();
    draw_forest();
    draw_logo();
    draw_lakes();
    draw_trees();
    draw_circuit();
    draw_coast();
}

static const char* SCENE_NAME[NUM_SCENES] = {"MODE 7", "ROTOZOOM + PRIORITY", "RASTER FX", "SPLIT SCREEN", "SPRITE STORM"};
static const char* SCENE_TITLE[NUM_SCENES] = {
    " 1/5 MODE 7 PERSPECTIVE + HDMA",
    " 2/5 ROTOZOOM + SPRITE PRIORITY",
    " 3/5 RASTER FX: WAVE + MIRROR",
    " 4/5 SPLIT SCREEN: 3 VIEWPORTS",
    " 5/5 SPRITE STORM: MULTIPLEX"};

static void load_palettes() {
    for (auto& p : BG_PAL)  set_pal(vram.bg.col, p.id, p.text);
    for (auto& p : FG_PAL)  set_pal(vram.fg.col, p.id, p.text);
    for (auto& p : SPR_PAL) set_pal(vram.spr_col, p.id, p.text);
}
// BG: tiles and the 64x64 world
static void load_bg() {
    for (auto& a : BG_ART) put_tile(vram.bg.tile, a.id, parse_tile(a.text));
    const uint8_t* shore = &vram.bg.tile[BT_SHORE_H * TILE_BYTES];    // transposed: sand left, water right
    put_tile(vram.bg.tile, BT_SHORE_V, make_tile([&](int x, int y) { return tile_pixel(shore, y, x); }));
    put_tile(vram.bg.tile, BT_SHORE_C, make_tile([](int x, int y) {      // water quarter bottom-right
        double d = hypot(x + 0.5 - 8, y + 0.5 - 8);
        return d < 4.0 ? 1 : d < 5.0 ? 3 : d < 6.0 ? 6 : (x * 3 + y * 5) % 7 == 0 ? 5 : 4;
    }));
    build_world();
}
// FG tiles: two fonts with a drop shadow, transparent + opaque
static void load_fg_tiles() {
    for (int ch = 32; ch <= 95; ++ch)
        for (int opaque = 0; opaque < 2; ++opaque)
            put_tile(vram.fg.tile, (opaque ? FT_OPAQUE : FT_FONT) + ch - 32, make_tile([&](int x, int y) {
                if (font_px(ch, x, y)) return 1 + y;
                bool shadow = x > 0 && y > 0 && font_px(ch, x - 1, y - 1);
                return opaque ? (shadow ? 14 : 15) : (shadow ? 9 : 0);
            }));
    for (auto& a : FG_ART) put_tile(vram.fg.tile, a.id, parse_tile(a.text));
}
// FG map (32 columns, mirrored at +32)
static void load_fg_map() {
    {   // rows 0..7: sky panorama
        std::vector<TilePixels> pool;
        for (int row = 0; row < 8; ++row)
            for (int col = 0; col < 32; ++col) {
                TilePixels p = make_tile([&](int x, int y) { return pano_px(col * 8 + x, row * 8 + y); });
                fg_cell(col, FR_SKY + row, fg_pano_tile(pool, p), FB_SKY);
            }
    }
    for (int c = 2; c < 32; c += 8) {                               // rows 10..17: colonnade (flips)
        fg_cell(c, FR_PILLARS, FT_CAP, FB_MARBLE, 0);
        fg_cell(c + 1, FR_PILLARS, FT_CAP, FB_MARBLE, 1);
        for (int r = 1; r <= 6; ++r) {
            fg_cell(c, FR_PILLARS + r, FT_SHAFT, FB_MARBLE, 0);
            fg_cell(c + 1, FR_PILLARS + r, FT_SHAFT, FB_MARBLE, 1);
        }
        fg_cell(c, FR_PILLARS + 7, FT_CAP, FB_MARBLE, 2);
        fg_cell(c + 1, FR_PILLARS + 7, FT_CAP, FB_MARBLE, 3);
    }
    fg_text(0, FR_TEXT_A, "** VERILOG PPU ** BG FG SPRITES ", FB_RAINBOW, false);
    fg_text(0, FR_TEXT_B, " SPIN: M00 < 0! ", FB_COOL, false);
    fg_bar(FR_LABEL + 0, " VIEW 1: ZOOM X4  FLIPS + BANKS", FB_LABEL);
    fg_bar(FR_LABEL + 1, " VIEW 2: ROTATION (AFFINE)", FB_LABEL);
    fg_bar(FR_LABEL + 2, " VIEW 3: SHEAR + V-STRETCH", FB_LABEL);
    fg_bar(FR_STATUS, "", FB_STATUS);
    fg_text(1, FR_STATUS, "FPS", FB_STATUS, true);                  // digits at cols 5,6   (sprites)
    fg_text(9, FR_STATUS, "SPRITES", FB_STATUS, true);              // digits at cols 17..19 (sprites)
    fg_text(23, FR_STATUS, "TAB:NEXT", FB_STATUS, true);
    for (int s = 0; s < NUM_SCENES; ++s) fg_bar(FR_TITLE + s, SCENE_TITLE[s], FB_TITLE);
    fg_text(0, FR_STORM, "181 SPRITES WITH ONLY 32 OAM *  ", FB_RAINBOW, false);   // 32 chars: loops as a marquee
}
// Sprite tiles: kit shapes (this demo has its own star), art, outlined digits
static void load_sprite_tiles() {
    for (int k = 0; k < 8; ++k) put_tile(vram.spr_tile, ST_SMILEY + k, parse_tile(KIT_SPRITES[k]));
    for (auto& a : SPR_ART) put_tile(vram.spr_tile, a.id, parse_tile(a.text));
    for (int d = 0; d < 10; ++d) {
        auto on = [&](int x, int y) { return x >= 0 && x < 8 && y >= 0 && y < 8 && font_px('0' + d, x, y); };
        put_tile(vram.spr_tile, ST_DIGIT0 + d, make_tile([&](int x, int y) {
            bool edge = false;
            for (int k = 0; k < 9; ++k) edge = edge || on(x + k % 3 - 1, y + k / 3 - 1);
            return on(x, y) ? 2 : edge ? 1 : 0;
        }));
    }
}
// The ONLY function that writes tiles, maps, attributes and palettes.
static void load_vram() {
    load_palettes();
    load_bg();
    load_fg_tiles();
    load_fg_map();
    load_sprite_tiles();
}
