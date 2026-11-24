// palette_page.h - PALETTE mode: the BG, FG and sprite col_rams (16 banks x 16 colors, RGB332)
#pragma once
#include "camera.h"

static uint8_t bank_clipboard[16];
static bool bank_copied = false;

static uint8_t* edited_bank() { return &scene.col[color_edit.ram][color_edit.bank * 16]; }

struct Rgb { int r, g, b; };
static Rgb split_rgb(uint8_t c) { return Rgb{c >> 5, (c >> 2) & 7, c & 3}; }

// Click on the view: select the color under the cursor
static void palette_view_input(bool over) {
    if (!take_click(mouse.left_click, over)) return;
    Point p = view_pixel();
    int i = p.y * PPU_W + p.x;
    const uint8_t index[3] = {captured.bg[i], captured.fg[i], captured.sprite[i]};
    color_edit.ram = mixer_winner(captured.fg[i], captured.sprite[i], captured.low_prio[i]);
    color_edit.bank = index[color_edit.ram] >> 4;
    color_edit.index = index[color_edit.ram] & 15;
    show_status("%s BANK %X INDEX %X", LAYER_NAME[color_edit.ram], color_edit.bank, color_edit.index);
}

// ---- Page ----
static void color_grid() {
    const int x0 = PICK_X + 18, y0 = PICK_Y;
    char label[64];
    snprintf(label, sizeof label, "%s COL_RAM (16 BANKS x 16)", LAYER_NAME[color_edit.ram]);
    panel(PICK_X, PICK_Y, 418, 400, label, col::CYAN);
    for (int bank = 0; bank < 16; ++bank) {
        snprintf(label, sizeof label, "%X", bank);
        draw_text(PICK_X + 2, y0 + bank * PICK_PITCH + 8, label, bank == color_edit.bank ? col::YELLOW : col::DIM);
        for (int i = 0; i < 16; ++i) {
            int x = x0 + i * PICK_PITCH, y = y0 + bank * PICK_PITCH;
            fill_rect(x, y, PICK_CELL, PICK_CELL, to_argb(scene.col[color_edit.ram][bank * 16 + i]));
            if (i == 0 && has_transparency(color_edit.ram))       // checkered corner: transparent
                for (int k = 0; k < 64; ++k)
                    if (k % 8 + k / 8 < 8) put_pixel(x + k % 8, y + k / 8, checker(k % 8 * 2, k / 8 * 2));
            if (!mouse_in(x, y, PICK_CELL, PICK_CELL)) continue;
            draw_rect(x - 1, y - 1, PICK_CELL + 2, PICK_CELL + 2, col::WHITE);
            if (take_click(mouse.left_click, true)) {
                color_edit.bank = bank;
                color_edit.index = i;
            }
        }
    }
    draw_selection(x0 + color_edit.index * PICK_PITCH, y0 + color_edit.bank * PICK_PITCH, PICK_CELL, PICK_CELL);
}

// Clickable levels of one channel (0 = R, 1 = G, 2 = B) of the selected color
static void channel_row(int x, int y, int channel) {
    static const char* const NAMES[3] = {"R", "G", "B"};
    const int levels = channel == 2 ? 4 : 8, w = 216 / levels;
    uint8_t& color = edited_bank()[color_edit.index];
    draw_text(x, y + 7, NAMES[channel], col::DIM);
    for (int level = 0; level < levels; ++level) {
        Rgb c = split_rgb(color);
        int& value = channel == 0 ? c.r : channel == 1 ? c.g : c.b;
        const bool selected = value == level;
        value = level;
        const uint8_t candidate = rgb332(c.r, c.g, c.b);
        const int bx = x + 24 + level * w;
        fill_rect(bx, y, w - 2, 22, to_argb(candidate));
        if (selected) draw_selection(bx, y, w - 2, 22);
        if (take_click(mouse.left_click, mouse_in(bx, y, w - 2, 22)) && !selected) {
            push_undo();
            color = candidate;
        }
    }
}

// Fill the colors between color 1 and the selected one
static void make_gradient() {
    push_undo();
    uint8_t* bank = edited_bank();
    const Rgb from = split_rgb(bank[1]), to = split_rgb(bank[color_edit.index]);
    const int n = color_edit.index - 1;
    for (int k = 1; k < n; ++k) {
        auto blend = [&](int a, int b) { return (a * (n - k) + b * k + n / 2) / n; };
        bank[1 + k] = rgb332(blend(from.r, to.r), blend(from.g, to.g), blend(from.b, to.b));
    }
}
static void bank_buttons(int x, int y) {
    uint8_t* bank = edited_bank();
    if (button(x, y, 120, 20, "COPY BANK")) {
        memcpy(bank_clipboard, bank, 16);
        bank_copied = true;
        show_status("BANK %X COPIED", color_edit.bank);
    }
    if (button(x + 128, y, 120, 20, "PASTE BANK") && bank_copied) {
        push_undo();
        memcpy(bank, bank_clipboard, 16);
    }
    if (button(x, y + 26, 248, 20, "BANK TO THE OTHER RAMS")) {
        push_undo();
        for (int ram = 0; ram < 3; ++ram)
            if (ram != color_edit.ram) memcpy(&scene.col[ram][color_edit.bank * 16], bank, 16);
        show_status("BANK %X COPIED TO ALL COL_RAMS", color_edit.bank);
    }
    if (button(x, y + 52, 248, 20, "GRADIENT 1 -> SELECTED") && color_edit.index > 2) make_gradient();
}

static void color_panel() {
    const int x = SIDE_X + 10, y = SIDE_Y;
    panel(x, y, 248, 300, "COLOR", col::PINK);
    for (int channel = 0; channel < 3; ++channel) channel_row(x, y + 108 + channel * 32, channel);
    bank_buttons(x, y + 212);

    const uint8_t color = edited_bank()[color_edit.index];
    const Rgb c = split_rgb(color);
    fill_rect(x, y, 248, 50, to_argb(color));
    draw_rect(x, y, 248, 50, col::BORDER);
    draw_textf(x, y + 58, col::TEXT, "%s BANK %X INDEX %X", LAYER_NAME[color_edit.ram], color_edit.bank, color_edit.index);
    draw_textf(x, y + 72, col::TEXT, "RGB332 0x%02X  R%d G%d B%d", color, c.r, c.g, c.b);
    if (color_edit.index == 0 && has_transparency(color_edit.ram))
        draw_text(x, y + 86, "INDEX 0 IS TRANSPARENT HERE", col::ORANGE);
}

static void palette_page() {
    draw_text(PAGE_X, TOOLBAR_Y + 5, "RAM", col::DIM);
    tabs(PAGE_X + 48, TOOLBAR_Y, 44, LAYER_NAME, 3, color_edit.ram);
    color_grid();
    color_panel();
}
