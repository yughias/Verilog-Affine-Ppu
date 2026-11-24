// tiles_page.h - TILES mode: pixel editor for the BG, FG and sprite tiles
#pragma once
#include "sprites_page.h"

constexpr int PIXEL_SIZE = SIDE_SIZE / 8;                     // a tile pixel in the editor
constexpr int COLORS_Y = SIDE_Y + 288, TILE_BUTTONS_Y = COLORS_Y + 64, PREVIEW_Y = TILE_BUTTONS_Y + 96;

static uint8_t tile_clipboard[TILE_BYTES];
static bool tile_copied = false;

static uint8_t* edited_tile() { return tile_ptr(tile_edit.set, tile_edit.tile[tile_edit.set]); }
static Point edited_pixel() { return Point{(mouse.x - SIDE_X) / PIXEL_SIZE, (mouse.y - SIDE_Y) / PIXEL_SIZE}; }

// ---- Mouse ----
// Click on the view: edit the tile under the cursor
static void tiles_view_input(bool over) {
    if (!take_click(mouse.left_click, over)) return;
    const int set = tile_edit.set;
    if (set == L_SPR) {
        int s = sprite_at(view_pixel());
        if (s >= 0) {
            tile_edit.tile[set] = scene.spr[s].tile;
            tile_edit.bank[set] = scene.spr[s].pal;
        }
    } else {
        Point c = view_cell(set);
        int i = cell_index(c.x, c.y);
        tile_edit.tile[set] = scene.map[set][i];
        tile_edit.bank[set] = scene.attr[set][i] >> 4;
    }
    show_status("TILE %d SELECTED", tile_edit.tile[set]);
}
static void drag_pixel() {
    Point p = edited_pixel();
    if (mouse_in_side()) set_tile_pixel(edited_tile(), p.x, p.y, tile_edit.color);
    if (!mouse.left) current_drag = DRAG_NONE;
}

// ---- Page ----
static void tiles_toolbar() {
    draw_text(PAGE_X, TOOLBAR_Y + 5, "SET", col::DIM);
    tabs(PAGE_X + 48, TOOLBAR_Y, 44, LAYER_NAME, 3, tile_edit.set);
    int& bank = tile_edit.bank[tile_edit.set];
    draw_text(PAGE_X, BANKS_Y + 5, "BANK", col::DIM);
    bank = bank_row(PAGE_X + 48, BANKS_Y, 22, tile_edit.set, bank);
}

static void pixel_editor(int set, int bank) {
    char title[32];
    snprintf(title, sizeof title, "PIXELS - TILE %d", tile_edit.tile[set]);
    panel(SIDE_X, SIDE_Y, SIDE_SIZE, SIDE_SIZE, title, col::ORANGE);
    fill_rect(SIDE_X, SIDE_Y, SIDE_SIZE, SIDE_SIZE, col::BORDER);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            draw_color(SIDE_X + x * PIXEL_SIZE + 1, SIDE_Y + y * PIXEL_SIZE + 1, PIXEL_SIZE - 1, PIXEL_SIZE - 1,
                       set, bank, tile_pixel(edited_tile(), x, y));
    if (!mouse_in_side()) return;
    Point p = edited_pixel();
    draw_rect(SIDE_X + p.x * PIXEL_SIZE, SIDE_Y + p.y * PIXEL_SIZE, PIXEL_SIZE + 1, PIXEL_SIZE + 1, col::WHITE);
    if (take_click(mouse.right_click, true)) tile_edit.color = tile_pixel(edited_tile(), p.x, p.y);
    if (take_click(mouse.left_click, true)) {
        push_undo();
        current_drag = DRAG_TILE;
    }
}

static void color_picker(int set, int bank) {
    panel(SIDE_X, COLORS_Y, SIDE_SIZE, 52, "COLOR", col::PINK);
    for (int c = 0; c < 16; ++c) {
        int x = SIDE_X + (c % 8) * 32, y = COLORS_Y + (c / 8) * 26;
        bool bright = (scene.col[set][bank * 16 + c] & 0x90) != 0;
        char label[4];
        snprintf(label, sizeof label, "%X", c);
        draw_color(x, y, 30, 24, set, bank, c);
        draw_text(x + 2, y + 2, label, bright ? col::BLACK : col::WHITE);
        if (c == tile_edit.color) draw_selection(x, y, 30, 24);
        if (take_click(mouse.left_click, mouse_in(x, y, 30, 24))) tile_edit.color = c;
    }
}

// Rearrange the pixels: pixel (x, y) of the new tile is pixel from(x, y) = row * 8 + column of the old one
template <class F>
static void remap_tile(F from) {
    push_undo();
    uint8_t* tile = edited_tile();
    TilePixels old = make_tile([&](int x, int y) { return tile_pixel(tile, x, y); });
    put_tile(tile, 0, make_tile([&](int x, int y) { return old[from(x, y)]; }));
}
static void tile_buttons(int set) {
    const int x = SIDE_X, y = TILE_BUTTONS_Y;
    uint8_t* tile = edited_tile();
    if (button(x, y, 60, 20, "CLEAR")) {
        push_undo();
        memset(tile, 0, TILE_BYTES);
    }
    if (button(x + 64, y, 60, 20, "FLIP H")) remap_tile([](int px, int py) { return py * 8 + 7 - px; });
    if (button(x + 128, y, 60, 20, "FLIP V")) remap_tile([](int px, int py) { return (7 - py) * 8 + px; });
    if (button(x + 192, y, 64, 20, "ROTATE")) remap_tile([](int px, int py) { return (7 - px) * 8 + py; });
    if (button(x, y + 24, 124, 20, "COPY")) {
        memcpy(tile_clipboard, tile, TILE_BYTES);
        tile_copied = true;
        show_status("TILE COPIED");
    }
    if (button(x + 132, y + 24, 124, 20, "PASTE") && tile_copied) {
        push_undo();
        memcpy(tile, tile_clipboard, TILE_BYTES);
    }
    // copy to the other two tile sets
    const int id = tile_edit.tile[set];
    int column = 0;
    for (int other = 0; other < 3; ++other) {
        if (other == set) continue;
        char label[24];
        snprintf(label, sizeof label, "TO %s %d", LAYER_NAME[other], id & (tile_count(other) - 1));
        if (button(x + 132 * column++, y + 48, 124, 20, label)) {
            push_undo();
            memcpy(tile_ptr(other, id), tile, TILE_BYTES);
            show_status("TILE %d COPIED TO %s", id, LAYER_NAME[other]);
        }
    }
}

// The tile repeated 3x3, to see how it joins with itself
static void tile_preview(int set, int bank) {
    panel(SIDE_X, PREVIEW_Y, 72, 72, "3X3", col::DIM);
    for (int k = 0; k < 9; ++k)
        draw_set_tile(SIDE_X + (k % 3) * 24, PREVIEW_Y + (k / 3) * 24, 3, set, tile_edit.tile[set], bank);
    const int x = SIDE_X + 84;
    if (set == L_SPR) draw_text(x, PREVIEW_Y + 4, "SPRITES: 64 TILES", col::DIM);
    if (has_transparency(set)) draw_text(x, PREVIEW_Y + 18, "COLOR 0 = TRANSPARENT", col::DIM);
    draw_text(x, PREVIEW_Y + 32, "BG/FG/SPR TILE RAMS", col::DIM);
    draw_text(x, PREVIEW_Y + 46, "ARE SEPARATE", col::DIM);
}

static void tiles_page() {
    tiles_toolbar();
    const int set = tile_edit.set, bank = tile_edit.bank[set];
    tile_edit.tile[set] = tile_picker_panel(set, bank, tile_edit.tile[set]);
    pixel_editor(set, bank);
    color_picker(set, bank);
    tile_buttons(set);
    tile_preview(set, bank);
}
