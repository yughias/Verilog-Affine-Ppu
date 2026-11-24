// view.h - the left half of the window: PPU output, layer and camera controls, help
#pragma once
#include "map_page.h"
#include "palette_page.h"
#include "sprites_page.h"
#include "tiles_page.h"

// ---- PPU output ----
static bool all_visible() { return layer_visible[L_BG] && layer_visible[L_FG] && layer_visible[L_SPR]; }

// The real VGA pins or, with some layer hidden, the mixer applied to the captured indices
static uint32_t view_color(int x, int y) {
    if (all_visible()) return vga_image[y * VGA_W + x];
    const int i = y * PPU_W + x;
    const uint8_t fg = layer_visible[L_FG] ? captured.fg[i] : 0;
    const uint8_t sprite = layer_visible[L_SPR] ? captured.sprite[i] : 0;
    switch (mixer_winner(fg, sprite, captured.low_prio[i])) {
        case L_SPR: return to_argb(scene.col[L_SPR][sprite]);
        case L_FG:  return to_argb(scene.col[L_FG][fg]);
    }
    return layer_visible[L_BG] ? to_argb(scene.col[L_BG][captured.bg[i]]) : checker(x * 2, y * 2);
}
static void draw_ppu_output() {
    panel(VIEW_X, VIEW_Y, VIEW_SIZE, VIEW_SIZE,
          all_visible() ? "PPU OUTPUT (VGA PINS)" : "PPU OUTPUT (SOFTWARE MIX, LAYERS HIDDEN)", col::CYAN);
    for (int y = 0; y < PPU_H; ++y)
        for (int x = 0; x < PPU_W; ++x)
            fill_rect(VIEW_X + x * VIEW_SCALE, VIEW_Y + y * VIEW_SCALE, VIEW_SCALE, VIEW_SCALE, view_color(x, y));
    for (int y = 0; y < PPU_H; ++y)          // red marks: too many sprites on the line
        if (sprites_on_line[y] > SPR_LINE_LIMIT) fill_rect(VIEW_X + VIEW_SIZE + 1, VIEW_Y + y * VIEW_SCALE, 3, 2, col::RED);
}
static void view_input() {
    const bool over = mouse_in_view();
    const bool space = SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_SPACE] != 0;
    if (over && mouse.wheel) zoom_camera(pow(1.1, mouse.wheel));
    if (take_click(mouse.middle_click, over)) current_drag = DRAG_PAN;
    if (space && take_click(mouse.left_click, over)) current_drag = DRAG_PAN;
    switch (current_mode) {
        case MODE_MAP:     map_view_input(over); break;
        case MODE_TILES:   tiles_view_input(over); break;
        case MODE_PALETTE: palette_view_input(over); break;
        case MODE_SPRITES: sprites_view_input(over); break;
    }
}

// ---- Below the view ----
static void view_controls() {
    const int x = VIEW_X, y = VIEW_Y + 524;
    for (int layer = 0; layer < 3; ++layer) {
        char label[16];
        snprintf(label, sizeof label, "%d %s", layer + 1, LAYER_NAME[layer]);
        bool& visible = layer_visible[layer];
        if (button(x + layer * 76, y, 72, 18, label, visible, col::GREEN)) visible = !visible;
    }
    if (button(x + 240, y, 88, 18, "L LINK", scene.linked, col::PURPLE)) toggle_camera_link();
    if (button(x + 334, y, 96, 18, "HOME RESET")) reset_camera();
    if (mouse_in_view()) draw_textf(x + 440, y + 5, col::DIM, "%3d,%3d", view_pixel().x, view_pixel().y);
    for (int layer = L_BG; layer <= L_FG; ++layer) {
        const Camera& c = scene.cam[layer];
        int degrees = ((int)lround(c.ang * 180 / PI) % 360 + 360) % 360;
        draw_textf(x, y + 28 + layer * 13, col::DIM, "%s CAMERA  X %6.1f  Y %6.1f  ROT %3d  ZOOM %4.2f",
                   LAYER_NAME[layer], c.cx, c.cy, degrees, c.zoom);
    }
}
static void view_help() {
    static const char* const COMMON[4] = {
        "F1-F4 MODE  CTRL+S SAVE  CTRL+O RELOAD  CTRL+N NEW",
        "CTRL+Z UNDO  CTRL+Y REDO  CTRL+E EXPORT .H  ESC QUIT",
        "WHEEL ZOOM  MIDDLE/SPACE+DRAG PAN  Q/E ROTATE  HOME RESET",
        "ARROWS PAN  L LINK CAMERAS  1/2/3 SHOW BG/FG/SPR",
    };
    static const char* const PER_MODE[NUM_MODES][3] = {
        {"LEFT: PAINT WITH THE TOOL  RIGHT: PICK  TAB: BG/FG",
         "[ ] TILE  , . BANK  X/Y FLIP",
         "MINIMAP: PAINT TOO, MIDDLE CLICK CENTERS THE CAMERA"},
        {"LEFT: PAINT PIXEL  RIGHT: PICK COLOR  TAB: BG/FG/SPR",
         "[ ] TILE  , . BANK",
         "CLICK THE VIEW: GRAB THE TILE UNDER THE CURSOR"},
        {"CLICK A COLOR CELL, THEN A R/G/B LEVEL",
         "TAB: BG/FG/SPR COLOR RAM",
         "CLICK THE VIEW: SELECT THE COLOR UNDER THE CURSOR"},
        {"CLICK THE VIEW: SELECT/DRAG, EMPTY SPOT MOVES SELECTED",
         "ARROWS NUDGE  TAB NEXT  [ ] TILE  , . PALETTE",
         "X/Y FLIP  P PRIORITY  DEL ON/OFF  OAM 0 IS ON TOP"},
    };
    const int x = VIEW_X, y = VIEW_Y + 600, line_h = 13;
    panel(x, y, VIEW_SIZE, 7 * line_h + 2, "HELP", col::DIM);
    for (int i = 0; i < 4; ++i) draw_text(x + 4, y + 4 + i * line_h, COMMON[i], col::DIM);
    for (int i = 0; i < 3; ++i) draw_text(x + 4, y + 4 + (4 + i) * line_h, PER_MODE[current_mode][i], col::TEXT);
}
