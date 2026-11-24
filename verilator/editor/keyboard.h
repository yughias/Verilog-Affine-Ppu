// keyboard.h - key presses and held keys
#pragma once
#include "files.h"
#include "view.h"

// True if there is nothing to lose, or the second time it is asked
static bool may_quit() {
    static uint32_t asked_at = 0;
    if (!unsaved || asked_twice(asked_at)) return true;
    show_status("UNSAVED CHANGES: DO IT AGAIN TO QUIT");
    return false;
}
static void ask_new_scenario() {
    static uint32_t asked_at = 0;
    if (!asked_twice(asked_at)) {
        show_status("PRESS CTRL+N AGAIN TO CLEAR EVERYTHING");
        return;
    }
    push_undo();
    new_scenario();
    show_status("NEW SCENARIO (CTRL+Z TO GO BACK)");
}

static void ctrl_key(SDL_Keycode key, bool shift, bool repeat) {
    if (key == SDLK_z && !shift) undo();
    if (key == SDLK_y || (key == SDLK_z && shift)) redo();
    if (repeat) return;
    if (key == SDLK_s) save_scenario();
    if (key == SDLK_o) reload_scenario();
    if (key == SDLK_e) export_scenario();
    if (key == SDLK_n) ask_new_scenario();
}

// Before changing the selected sprite: one undo step per key press, even if it auto-repeats
static void begin_sprite_key(const SDL_KeyboardEvent& k) {
    if (!k.repeat) push_undo();
    unsaved = true;
}

// Keys that may auto-repeat
static void nudge_key(const SDL_KeyboardEvent& k, bool shift) {
    const SDL_Keycode key = k.keysym.sym;
    const int dx = (key == SDLK_RIGHT) - (key == SDLK_LEFT), dy = (key == SDLK_DOWN) - (key == SDLK_UP);
    if (current_mode != MODE_SPRITES || (!dx && !dy)) return;
    begin_sprite_key(k);
    Sprite& s = edited_sprite();
    s.x += dx * (shift ? 8 : 1);
    s.y += dy * (shift ? 8 : 1);
    clamp_sprite(s);
}
static void next_previous_key(const SDL_KeyboardEvent& k) {
    const SDL_Keycode key = k.keysym.sym;
    const int tile = (key == SDLK_RIGHTBRACKET) - (key == SDLK_LEFTBRACKET);
    const int bank = (key == SDLK_PERIOD) - (key == SDLK_COMMA);
    if (!tile && !bank) return;
    auto step = [](int& value, int delta, int count) { value = (value + delta) & (count - 1); };
    switch (current_mode) {
        case MODE_MAP:
            step(brush.tile, tile, LAYER_TILES);
            step(brush.bank, bank, 16);
            break;
        case MODE_TILES:
            step(tile_edit.tile[tile_edit.set], tile, tile_count(tile_edit.set));
            step(tile_edit.bank[tile_edit.set], bank, 16);
            break;
        case MODE_PALETTE:
            step(color_edit.bank, bank, 16);
            break;
        case MODE_SPRITES:
            begin_sprite_key(k);
            step(edited_sprite().tile, tile, SPR_TILES);
            step(edited_sprite().pal, bank, 16);
            break;
    }
}

// Keys that work once per press
static void tab_key(bool shift) {
    auto cycle = [&](int& value, int count) { value = (value + (shift ? count - 1 : 1)) % count; };
    switch (current_mode) {
        case MODE_MAP:     cycle(brush.layer, 2); break;
        case MODE_TILES:   cycle(tile_edit.set, 3); break;
        case MODE_PALETTE: cycle(color_edit.ram, 3); break;
        case MODE_SPRITES: cycle(selected_sprite, NUM_OAM); break;
    }
}
static void flip_key(int flip_bit) {
    if (current_mode == MODE_MAP) brush.flip ^= flip_bit;
}
static void sprite_toggle_key(const SDL_KeyboardEvent& k, bool Sprite::*property) {
    if (current_mode != MODE_SPRITES) return;
    begin_sprite_key(k);
    Sprite& s = edited_sprite();
    s.*property = !(s.*property);
}
static void command_key(const SDL_KeyboardEvent& k, bool shift) {
    const SDL_Keycode key = k.keysym.sym;
    switch (key) {
        case SDLK_F1: case SDLK_F2: case SDLK_F3: case SDLK_F4: current_mode = (int)(key - SDLK_F1); break;
        case SDLK_1: case SDLK_2: case SDLK_3: layer_visible[key - SDLK_1] = !layer_visible[key - SDLK_1]; break;
        case SDLK_l: toggle_camera_link(); break;
        case SDLK_HOME: reset_camera(); break;
        case SDLK_TAB: tab_key(shift); break;
        case SDLK_x: flip_key(1); sprite_toggle_key(k, &Sprite::fx); break;
        case SDLK_y: flip_key(2); sprite_toggle_key(k, &Sprite::fy); break;
        case SDLK_p: sprite_toggle_key(k, &Sprite::lo); break;
        case SDLK_DELETE:
        case SDLK_BACKSPACE: sprite_toggle_key(k, &Sprite::on); break;
        default: break;
    }
}

// Returns false to quit
static bool on_key(const SDL_KeyboardEvent& k) {
    const bool shift = (k.keysym.mod & KMOD_SHIFT) != 0, ctrl = (k.keysym.mod & KMOD_CTRL) != 0;
    if (ctrl) {
        ctrl_key(k.keysym.sym, shift, k.repeat != 0);
        return true;
    }
    nudge_key(k, shift);
    next_previous_key(k);
    if (k.repeat) return true;
    if (k.keysym.sym == SDLK_ESCAPE) return !may_quit();
    command_key(k, shift);
    return true;
}

// Held keys: camera
static void held_keys() {
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    if (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL]) return;
    const int dx = keys[SDL_SCANCODE_LEFT] - keys[SDL_SCANCODE_RIGHT];
    const int dy = keys[SDL_SCANCODE_UP] - keys[SDL_SCANCODE_DOWN];
    const int turn = keys[SDL_SCANCODE_E] - keys[SDL_SCANCODE_Q];
    if (current_mode != MODE_SPRITES && (dx || dy)) pan_camera(3 * dx, 3 * dy);
    if (turn) rotate_camera(0.02 * turn);
}
