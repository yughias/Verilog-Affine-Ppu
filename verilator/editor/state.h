// state.h - what the editor is doing: mode, selections, status message, file, undo history
#pragma once
#include <string>
#include "scenario.h"

enum { MODE_MAP = 0, MODE_TILES, MODE_PALETTE, MODE_SPRITES, NUM_MODES };
enum { TOOL_PEN = 0, TOOL_RECT, TOOL_FILL, TOOL_PICK, TOOL_ERASE, NUM_TOOLS };
enum { DRAG_NONE = 0, DRAG_PAN, DRAG_MAP, DRAG_TILE, DRAG_SPRITE };   // what the held mouse button does

static const char* const MODE_NAME[NUM_MODES] = {"F1 MAP", "F2 TILES", "F3 PALETTE", "F4 SPRITES"};
static const char* const TOOL_NAME[NUM_TOOLS] = {"PEN", "RECT", "FILL", "PICK", "ERASE"};
static const char* const LAYER_NAME[3] = {"BG", "FG", "SPR"};

static int current_mode = MODE_MAP;
static int current_drag = DRAG_NONE;
static bool layer_visible[3] = {true, true, true};

// ---- Selections of each mode ----
struct Brush {              // MAP: what is painted, where and how
    int layer = L_BG, tool = TOOL_PEN;
    int tile = T_GRASS, bank = 1, flip = 0;
};
struct TileSelection {      // TILES: the edited set, and tile / bank of each set
    int set = L_BG;
    int tile[3] = {T_GRASS, T_GRASS, 0};
    int bank[3] = {1, 1, 1};
    int color = 1;
};
struct ColorSelection {     // PALETTE
    int ram = L_BG, bank = 0, index = 1;
};
static Brush brush;
static TileSelection tile_edit;
static ColorSelection color_edit;
static int selected_sprite = 0;

// ---- Status message (top right, 5 seconds) ----
constexpr uint32_t STATUS_MS = 5000;
static char status_text[160] = "";
static uint32_t status_time = 0;

static void show_status(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(status_text, sizeof status_text, fmt, args);
    va_end(args);
    status_time = SDL_GetTicks();
}

// ---- File ----
static std::string file_path = "scenario.ppus";
static bool unsaved = false;

// Dangerous actions must be asked twice within 3 seconds; asked_at = time of the first request
static bool asked_twice(uint32_t& asked_at) {
    uint32_t now = SDL_GetTicks();
    bool second = asked_at && now - asked_at < 3000;
    asked_at = second ? 0 : now;
    return second;
}

// ---- Undo: whole copies of the scenario ----
constexpr size_t MAX_UNDO = 100;
static std::vector<Scenario> undo_stack, redo_stack;

// Call before changing the scenario
static void push_undo() {
    undo_stack.push_back(scene);
    if (undo_stack.size() > MAX_UNDO) undo_stack.erase(undo_stack.begin());
    redo_stack.clear();
    unsaved = true;
}
// Go back to the last scenario in `from`, saving the current one in `to`. Cameras do not move.
static void restore(std::vector<Scenario>& from, std::vector<Scenario>& to, const char* name) {
    if (from.empty()) {
        show_status("NOTHING TO %s", name);
        return;
    }
    Scenario previous = from.back();
    from.pop_back();
    previous.cam[L_BG] = scene.cam[L_BG];
    previous.cam[L_FG] = scene.cam[L_FG];
    previous.linked = scene.linked;
    to.push_back(scene);
    scene = previous;
    unsaved = true;
    show_status("%s (%d LEFT)", name, (int)from.size());
}
static void undo() { restore(undo_stack, redo_stack, "UNDO"); }
static void redo() { restore(redo_stack, undo_stack, "REDO"); }
