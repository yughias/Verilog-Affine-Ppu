// widgets.h - screen layout, mouse and immediate-mode widgets (drawn and handled in one call)
#pragma once
#include "state.h"

// ---- Layout (canvas pixels) ----
constexpr int CANVAS_W = 1280, CANVAS_H = 800;
constexpr int VIEW_X = 16, VIEW_Y = 52, VIEW_SCALE = 2;      // PPU output
constexpr int VIEW_SIZE = PPU_W * VIEW_SCALE;
constexpr int PAGE_X = 556, TOOLBAR_Y = 44, BANKS_Y = 73;    // right half: the page of the mode
constexpr int PICK_X = 562, PICK_Y = 128;                    // tile picker, color grid, OAM list
constexpr int SIDE_X = 990, SIDE_Y = 128, SIDE_SIZE = 256;   // minimap, pixel editor, properties
constexpr int PICK_PITCH = 25, PICK_CELL = 24;               // cells of tile picker and color grid

// ---- Mouse ----
struct Mouse {
    int x = -1, y = -1, prev_x = -1, prev_y = -1;                           // this frame / previous frame
    bool left = false, right = false, middle = false;                       // held
    bool left_click = false, right_click = false, middle_click = false;     // pressed in this frame
    int wheel = 0;
};
static Mouse mouse;

static inline bool mouse_in(int x, int y, int w, int h) {
    return mouse.x >= x && mouse.y >= y && mouse.x < x + w && mouse.y < y + h;
}
static inline bool mouse_in_view() { return mouse_in(VIEW_X, VIEW_Y, VIEW_SIZE, VIEW_SIZE); }
static inline bool mouse_in_side() { return mouse_in(SIDE_X, SIDE_Y, SIDE_SIZE, SIDE_SIZE); }

// Consume a click if the mouse is over the widget: nobody else will see it
static bool take_click(bool& click, bool over) {
    bool hit = click && over;
    if (hit) click = false;
    return hit;
}

// ---- Buttons ----
static bool button(int x, int y, int w, int h, const char* label, bool active = false, uint32_t accent = col::CYAN) {
    bool hover = mouse_in(x, y, w, h);
    uint32_t background = active ? mix_color(col::PANEL, accent, 70) : hover ? 0xFF1C2433 : 0xFF141A24;
    fill_rect(x, y, w, h, background);
    draw_rect(x, y, w, h, active ? accent : hover ? col::DIM : col::BORDER);
    int text_w = (int)strlen(label) * 8;
    draw_text(x + (w - text_w) / 2, y + (h - 8) / 2, label, active ? col::WHITE : col::TEXT);
    return take_click(mouse.left_click, hover);
}
// Row of buttons that selects one of n values
static void tabs(int x, int y, int w, const char* const* names, int n, int& selected, uint32_t accent = col::CYAN) {
    for (int i = 0; i < n; ++i)
        if (button(x + i * (w + 4), y, w, 18, names[i], selected == i, accent)) selected = i;
}
// The 16 palette banks, each with a strip of its colors. Returns the selected bank.
static int bank_row(int x, int y, int w, int ram, int bank) {
    for (int b = 0; b < 16; ++b) {
        int bx = x + b * (w + 2);
        char label[4];
        snprintf(label, sizeof label, "%X", b);
        if (button(bx, y, w, 16, label, b == bank)) bank = b;
        for (int i = 0; i < w; ++i) fill_rect(bx + i, y + 17, 1, 4, to_argb(scene.col[ram][b * 16 + (i * 16) / w]));
    }
    return bank;
}

// ---- Tiles and colors ----
// Color 0 of the FG and of the sprites is transparent
static inline bool has_transparency(int set) { return set != L_BG; }

static void draw_set_tile(int x, int y, int scale, int set, int tile, int bank, int flip = 0) {
    draw_tile(x, y, scale, tile_ptr(set, tile), scene.col[set], bank, flip, has_transparency(set));
}
// Rectangle of one color (a checkerboard if it is transparent)
static void draw_color(int x, int y, int w, int h, int set, int bank, int index) {
    if (index != 0 || !has_transparency(set)) {
        fill_rect(x, y, w, h, to_argb(scene.col[set][bank * 16 + index]));
        return;
    }
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i) put_pixel(x + i, y + j, checker(i, j));
}
// Two-pixel outline around a selected cell
static void draw_selection(int x, int y, int w, int h) {
    draw_rect(x - 1, y - 1, w + 2, h + 2, col::YELLOW);
    draw_rect(x - 2, y - 2, w + 4, h + 4, col::YELLOW);
}
// Grid of all the tiles of a set. Returns the selected tile (changes with a click).
static int tile_picker(int x, int y, int set, int bank, int selected, int columns) {
    const int count = tile_count(set);
    int hovered = -1;
    auto cell_x = [&](int tile) { return x + (tile % columns) * PICK_PITCH; };
    auto cell_y = [&](int tile) { return y + (tile / columns) * PICK_PITCH; };
    auto outline = [&](int tile, uint32_t c) {
        draw_rect(cell_x(tile) - 1, cell_y(tile) - 1, PICK_CELL + 2, PICK_CELL + 2, c);
    };
    for (int tile = 0; tile < count; ++tile) {
        draw_set_tile(cell_x(tile), cell_y(tile), 3, set, tile, bank);
        if (mouse_in(cell_x(tile), cell_y(tile), PICK_CELL, PICK_CELL)) hovered = tile;
    }
    outline(selected, col::YELLOW);
    if (hovered < 0) return selected;
    outline(hovered, col::WHITE);
    draw_textf(x, y + (count / columns) * PICK_PITCH + 6, col::DIM, "TILE %3d (0x%02X)", hovered, hovered);
    return take_click(mouse.left_click, true) ? hovered : selected;
}
// Tile picker of the MAP and TILES pages
static int tile_picker_panel(int set, int bank, int selected) {
    char title[64];
    snprintf(title, sizeof title, "%s TILES (BANK %X)", LAYER_NAME[set], bank);
    panel(PICK_X, PICK_Y, 16 * PICK_PITCH, tile_count(set) / 16 * PICK_PITCH, title, col::CYAN);
    return tile_picker(PICK_X, PICK_Y, set, bank, selected, 16);
}
