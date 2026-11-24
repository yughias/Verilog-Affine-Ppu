// sprites_page.h - SPRITES mode: the 32 OAM entries
#pragma once
#include "camera.h"

static int sprites_on_line[PPU_H];

static Sprite& edited_sprite() { return scene.spr[selected_sprite]; }
static int flip_of(const Sprite& s) { return (s.fx ? 1 : 0) | (s.fy ? 2 : 0); }

static void count_sprites_on_lines() {
    memset(sprites_on_line, 0, sizeof sprites_on_line);
    for (const Sprite& s : scene.spr) {
        if (sprite_oam(s) == OAM_HIDDEN) continue;
        for (int y = std::max(0, s.y); y < std::min(PPU_H, s.y + 8); ++y) ++sprites_on_line[y];
    }
}
// Topmost sprite at a screen pixel, -1 = none
static int sprite_at(Point p) {
    for (int i = 0; i < NUM_OAM; ++i) {
        const Sprite& s = scene.spr[i];
        if (s.on && p.x >= s.x && p.x < s.x + 8 && p.y >= s.y && p.y < s.y + 8) return i;
    }
    return -1;
}

// ---- Mouse ----
static Point grab_offset;   // from the sprite corner to the grabbed pixel

// Click on the view: grab a sprite, or bring the selected one here
static void sprites_view_input(bool over) {
    if (take_click(mouse.left_click, over)) {
        push_undo();
        Point p = view_pixel();
        int hit = sprite_at(p);
        if (hit >= 0) selected_sprite = hit;
        Sprite& s = edited_sprite();
        if (hit < 0) {
            s.x = p.x - 4;
            s.y = p.y - 4;
            s.on = true;
            clamp_sprite(s);
        }
        grab_offset = Point{p.x - s.x, p.y - s.y};
        current_drag = DRAG_SPRITE;
    }
    for (int i = NUM_OAM - 1; i >= 0; --i) {
        const Sprite& s = scene.spr[i];
        uint32_t c = i == selected_sprite ? col::YELLOW : mix_color(col::BG, col::DIM, 140);
        if (s.on) draw_rect(VIEW_X + s.x * VIEW_SCALE - 1, VIEW_Y + s.y * VIEW_SCALE - 1, 18, 18, c);
    }
}
static void drag_sprite() {
    Point p = view_pixel();
    Sprite& s = edited_sprite();
    s.x = p.x - grab_offset.x;
    s.y = p.y - grab_offset.y;
    clamp_sprite(s);
    if (!mouse.left) current_drag = DRAG_NONE;
}

// ---- Page ----
static void oam_list() {
    const int x = PICK_X, w = 400, row_h = 16;
    panel(x, PICK_Y, w, NUM_OAM * row_h, "OAM (ENTRY 0 IS DRAWN ON TOP)", col::CYAN);
    for (int i = 0; i < NUM_OAM; ++i) {
        const Sprite& s = scene.spr[i];
        const int y = PICK_Y + i * row_h;
        const bool hover = mouse_in(x, y, w, row_h);
        const bool off_screen = s.on && sprite_oam(s) == OAM_HIDDEN;
        if (i == selected_sprite) fill_rect(x, y, w, row_h, mix_color(col::PANEL, col::YELLOW, 40));
        else if (hover) fill_rect(x, y, w, row_h, 0xFF1C2433);
        draw_set_tile(x + 2, y + 4, 1, L_SPR, s.tile, s.pal, flip_of(s));
        draw_textf(x + 16, y + 4, s.on ? col::TEXT : col::DIM, "%02d %-3s X%4d Y%4d T%02d P%X %s %s %s%s",
                   i, s.on ? "ON" : "off", s.x, s.y, s.tile, s.pal, s.fx ? "FX" : "--", s.fy ? "FY" : "--",
                   s.lo ? "LO" : "hi", off_screen ? " OUT" : "");
        if (take_click(mouse.left_click, hover)) selected_sprite = i;
    }
}

static void toggle_button(int x, int y, int w, const char* label, bool& value, uint32_t accent) {
    if (!button(x, y, w, 20, label, value, accent)) return;
    push_undo();
    value = !value;
}
// Change the order of the selected sprite (= its priority)
static void move_in_oam(int direction) {
    int other = selected_sprite + direction;
    if (other < 0 || other >= NUM_OAM) return;
    push_undo();
    std::swap(scene.spr[selected_sprite], scene.spr[other]);
    selected_sprite = other;
}
static void select_next_free() {
    for (int k = 1; k <= NUM_OAM; ++k) {
        int i = (selected_sprite + k) % NUM_OAM;
        if (scene.spr[i].on) continue;
        selected_sprite = i;
        return;
    }
}
static void sprite_properties(int x) {
    Sprite& s = edited_sprite();
    panel(x, SIDE_Y, 200, 200, "SPRITE TILES", col::ORANGE);
    int tile = tile_picker(x, SIDE_Y, L_SPR, s.pal, s.tile, 8);
    int y = SIDE_Y + 236;
    draw_text(x, y - 12, "PALETTE", col::DIM);
    int pal = bank_row(x, y, 10, L_SPR, s.pal);
    if (tile != s.tile || pal != s.pal) {
        push_undo();
        s.tile = tile;
        s.pal = pal;
    }
    y += 34;
    toggle_button(x, y, 46, "ON", s.on, col::GREEN);
    toggle_button(x + 50, y, 46, "FX", s.fx, col::PINK);
    toggle_button(x + 100, y, 46, "FY", s.fy, col::PINK);
    toggle_button(x + 150, y, 50, "LO", s.lo, col::PURPLE);
    y += 26;
    if (button(x, y, 46, 20, "UP")) move_in_oam(-1);
    if (button(x + 50, y, 46, 20, "DOWN")) move_in_oam(1);
    if (button(x + 100, y, 100, 20, "NEXT FREE")) select_next_free();
}

static void sprite_info(int x, int y) {
    const Sprite& s = edited_sprite();
    draw_set_tile(x, y, 6, L_SPR, s.tile, s.pal, flip_of(s));
    draw_rect(x - 1, y - 1, 50, 50, col::BORDER);
    draw_textf(x + 60, y, col::TEXT, "SPRITE %d", selected_sprite);
    draw_textf(x + 60, y + 14, col::TEXT, "X %d  Y %d", s.x, s.y);
    draw_textf(x + 60, y + 28, col::DIM, "OAM 0x%08X", (unsigned)sprite_oam(s));
}
static void line_limit_info(int x, int y) {
    int used = 0, crowded = 0, first = -1, worst = 0;
    for (const Sprite& s : scene.spr) used += s.on;
    for (int line = 0; line < PPU_H; ++line) {
        worst = std::max(worst, sprites_on_line[line]);
        if (sprites_on_line[line] <= SPR_LINE_LIMIT) continue;
        ++crowded;
        if (first < 0) first = line;
    }
    draw_textf(x, y, col::TEXT, "SPRITES ON: %d / %d", used, NUM_OAM);
    draw_textf(x, y + 14, col::TEXT, "MAX ON ONE LINE: %d (LIMIT %d)", worst, SPR_LINE_LIMIT);
    if (!crowded) {
        draw_text(x, y + 28, "NO LINE OVER THE LIMIT", col::GREEN);
        return;
    }
    draw_textf(x, y + 28, col::RED, "%d LINES OVER THE LIMIT", crowded);
    draw_textf(x, y + 42, col::RED, "FIRST AT Y=%d (RED MARKS)", first);
}

static void sprites_page() {
    const int x = SIDE_X + 20;
    oam_list();
    sprite_properties(x);
    sprite_info(x, SIDE_Y + 336);
    line_limit_info(x, SIDE_Y + 398);
}
