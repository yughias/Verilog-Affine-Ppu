// assets.h - font, colors, tile / palette text format and the starter art shared by the programs
#pragma once

#include <array>
#include "ppu_sim.h"

// ---- Font: 8x8, ASCII 32..127, one word per glyph (byte y = row y, bit x = column x) ----
static const uint64_t FONT[96] = {
    0x0000000000000000, 0x00180018183C3C18, 0x0000000000003636, 0x0036367F367F3636,
    0x000C1F301E033E0C, 0x0063660C18336300, 0x006E333B6E1C361C, 0x0000000000030606,
    0x00180C0606060C18, 0x00060C1818180C06, 0x0000663CFF3C6600, 0x00000C0C3F0C0C00,
    0x060C0C0000000000, 0x000000003F000000, 0x000C0C0000000000, 0x000103060C183060,
    0x003E676F7B73633E, 0x003F0C0C0C0C0E0C, 0x003F33061C30331E, 0x001E33301C30331E,
    0x0078307F33363C38, 0x001E3330301F033F, 0x001E33331F03061C, 0x000C0C0C1830333F,
    0x001E33331E33331E, 0x000E18303E33331E, 0x000C0C00000C0C00, 0x060C0C00000C0C00,
    0x00180C0603060C18, 0x00003F00003F0000, 0x00060C1830180C06, 0x000C000C1830331E,
    0x001E037B7B7B633E, 0x0033333F33331E0C, 0x003F66663E66663F, 0x003C66030303663C,
    0x001F36666666361F, 0x007F46161E16467F, 0x000F06161E16467F, 0x007C66730303663C,
    0x003333333F333333, 0x001E0C0C0C0C0C1E, 0x001E333330303078, 0x006766361E366667,
    0x007F66460606060F, 0x0063636B7F7F7763, 0x006363737B6F6763, 0x001C36636363361C,
    0x000F06063E66663F, 0x00381E3B3333331E, 0x006766363E66663F, 0x001E33380E07331E,
    0x001E0C0C0C0C2D3F, 0x003F333333333333, 0x000C1E3333333333, 0x0063777F6B636363,
    0x0063361C1C366363, 0x001E0C0C1E333333, 0x007F664C1831637F, 0x001E06060606061E,
    0x00406030180C0603, 0x001E18181818181E, 0x0000000063361C08, 0xFF00000000000000,
    0x0000000000180C0C, 0x006E333E301E0000, 0x003B66663E060607, 0x001E3303331E0000,
    0x006E33333E303038, 0x001E033F331E0000, 0x000F06060F06361C, 0x1F303E33336E0000,
    0x006766666E360607, 0x001E0C0C0C0E000C, 0x1E33333030300030, 0x0067361E36660607,
    0x001E0C0C0C0C0C0E, 0x00636B7F7F330000, 0x00333333331F0000, 0x001E3333331E0000,
    0x0F063E66663B0000, 0x78303E33336E0000, 0x000F06666E3B0000, 0x001F301E033E0000,
    0x00182C0C0C3E0C08, 0x006E333333330000, 0x000C1E3333330000, 0x00367F7F6B630000,
    0x0063361C36630000, 0x1F303E3333330000, 0x003F260C193F0000, 0x00380C0C070C0C38,
    0x0018181800181818, 0x00070C0C380C0C07, 0x0000000000003B6E, 0xFFFFFFFFFFFFFFFF,
};
static inline bool font_px(int ch, int x, int y) { return (FONT[ch - 32] >> (y * 8 + x)) & 1; }

// ---- Colors ----
static constexpr uint8_t rgb332(int r, int g, int b) { return (uint8_t)(((r & 7) << 5) | ((g & 7) << 2) | (b & 3)); }
// RGB332 -> ARGB exactly like ppu.v: {r,r,r[1:0]} {g,g,g[1:0]} {b,b,b,b}
static inline uint32_t to_argb(uint8_t c) {
    uint32_t r = c >> 5, g = (c >> 2) & 7, b = c & 3;
    return 0xFF000000u | (((r << 5) | (r << 2) | (r & 3)) << 16) | (((g << 5) | (g << 2) | (g & 3)) << 8) | (b * 0x55);
}
// Palette bank from text: one "rgb" triple per color, r and g 0..7, b 0..3
static inline void set_pal(uint8_t* col, int bank, const char* s) {
    for (int i = 0; i < 16; s += 3, ++i) {
        while (*s == ' ') ++s;
        if (!*s) return;
        col[bank * 16 + i] = rgb332(s[0] - '0', s[1] - '0', s[2] - '0');
    }
}

// ---- Tiles ----
using TilePixels = std::array<uint8_t, 64>;   // one palette index per pixel, row by row

// Tile from text: 64 hex digits, '.' = color 0, spaces ignored
static inline TilePixels parse_tile(const char* s) {
    TilePixels p{};
    for (int n = 0; *s && n < 64; ++s)
        if (*s != ' ') p[n++] = (uint8_t)(*s == '.' ? 0 : *s >= 'a' ? *s - 'a' + 10 : *s - '0');
    return p;
}
// Tile from a function f(x, y) -> color
template <class F>
static inline TilePixels make_tile(F f) {
    TilePixels p{};
    for (int i = 0; i < 64; ++i) p[i] = (uint8_t)f(i & 7, i >> 3);
    return p;
}
static inline uint8_t tile_pixel(const uint8_t* t, int x, int y) {   // pixel of a tile in memory
    uint8_t b = t[y * 4 + (x >> 1)];
    return (x & 1) ? (b >> 4) : (b & 15);
}
static inline void set_tile_pixel(uint8_t* t, int x, int y, int c) {
    uint8_t& b = t[y * 4 + (x >> 1)];
    b = (x & 1) ? (uint8_t)((b & 0x0F) | ((c & 15) << 4)) : (uint8_t)((b & 0xF0) | (c & 15));
}
static inline void put_tile(uint8_t* mem, int id, const TilePixels& p) {
    for (int i = 0; i < 64; ++i) set_tile_pixel(mem + id * TILE_BYTES, i & 7, i >> 3, p[i]);
}
// Characters 32..127 into the tiles first.., f(on, row) -> color
template <class F>
static inline void load_font(uint8_t* mem, int first, F f) {
    for (int ch = 32; ch <= 127; ++ch)
        put_tile(mem, first + ch - 32, make_tile([&](int x, int y) { return f(font_px(ch, x, y), y); }));
}

// ---- Starter kit ----
enum { T_GRASS = 128, T_WATER, T_TREE, T_TRUNK, T_BRICK, T_COIN, T_CHECKER };   // tile ids of KIT_TILES
static const char* const KIT_TILES[7] = {
    "..1..... .1...2.. ......1. 2..3.... .....1.. .1.....2 ...2..1. ....1...",   // grass
    "44555444 45665544 55544455 44445565 44555444 56654445 45444554 44455444",   // water
    ".....299 ...299aa ..29aa99 .29999a9 29a99999 29992999 .2299992 ...22922",   // tree, left half
    "......78 ......78 ......78 .....778 ....7788 ..277888 .2222222 ........",   // trunk, left half
    "33334333 32214321 32114221 44444444 33433334 21432214 21422114 44444444",   // brick
    "..ffff.. .f6676f. f675566f f676566f f676566f f665566f .f6665f. ..ffff..",   // coin
    "eeeebbbb edddbccc edddbccc edddbccc bbbbeeee bccceddd bccceddd bccceddd",   // checker
};
static const char* const KIT_SPRITES[8] = {   // 1 outline, 2 main, 3 highlight, 4 accent
    "..1111.. .123221. 12122121 12222221 12422421 12244221 .122221. ..1111..",   // smiley
    ".11..11. 12211221 12322221 12222221 .122221. ..1221.. ...11... ........",   // heart
    "...11... ..1221.. 11123111 12222221 .122221. .121121. 121..121 11....11",   // star
    "....1... ....11.. 1111121. 12222231 14444441 1111141. ....11.. ....1...",   // arrow
    "..1111.. .122221. 12342341 12222221 12222221 12222221 12122121 1.1..1.1",   // ghost
    "...11... ..1321.. .132221. 13222241 12222441 .122441. ..1441.. ...11...",   // diamond
    "..1111.. .123321. 12332221 12222331 11111111 .141141. .144441. ..1111..",   // mushroom
    "1111111. 1322221. 1211111. 122241.. 121111.. 121..... 141..... 111.....",   // letter F
};
static const char* const PAL_RAINBOW = "001 711 730 760 271 063 143 423 733 773 552 331 751 420 032 773";
static const char* const PAL_WORLD   = "140 260 030 761 023 043 363 310 520 050 371 221 442 663 710 500";
static const char* const PAL_DUNGEON = "101 410 620 741 210 530 760 773 600 730 770 043 473 332 663 000";
static const char* const KIT_SPRITE_PALS[4] = {   // sprite banks 1..4: yellow, red, blue, green
    "000 000 760 773 510", "000 000 600 742 770", "000 000 023 353 773", "000 000 160 471 502"};

static inline void load_kit_tiles(uint8_t* mem) {
    for (int k = 0; k < 7; ++k) put_tile(mem, T_GRASS + k, parse_tile(KIT_TILES[k]));
}
static inline void load_kit_sprites(uint8_t* tiles, uint8_t* col, int first_tile = 0) {
    for (int k = 0; k < 8; ++k) put_tile(tiles, first_tile + k, parse_tile(KIT_SPRITES[k]));
    for (int p = 0; p < 4; ++p) set_pal(col, p + 1, KIT_SPRITE_PALS[p]);
}
