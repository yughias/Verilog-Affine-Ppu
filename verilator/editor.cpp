// editor.cpp - scenario editor for the PPU
//
// Build your own scenario (both tile layers, tile graphics, palettes, sprites and the
// camera of each layer) while the real RTL (ppu.v, simulated by Verilator) renders it
// live. The C++ side keeps the whole scenario in a Scenario struct; before every frame it
// is copied into the Verilator memories, so what you see is exactly what the hardware
// draws.
//
// Modes (F1..F4 or the tabs at the top)
//   MAP      paint the 64x64 BG / FG tilemaps (pen, rectangle, flood fill, picker, eraser),
//            choose tile, palette bank and flip; paint on the PPU view or on the minimap
//   TILES    pixel editor for the 256 BG tiles, the 256 FG tiles and the 64 sprite tiles
//   PALETTE  edit the three col_rams (BG, FG, sprites): 16 banks x 16 colors, RGB332
//   SPRITES  the 32 OAM entries: position (drag on the view), tile, palette, flip,
//            low priority, order (OAM 0 is drawn on top); warns about the per-line limit
//
// Controls (all modes)
//   F1..F4 mode          CTRL+S save     CTRL+O reload     CTRL+N new (press twice)
//   CTRL+Z undo          CTRL+Y redo     CTRL+E export a C header with all the data
//   WHEEL zoom           MIDDLE drag / SPACE+LEFT drag: pan        Q / E rotate
//   ARROWS pan (SPRITES mode: nudge the selected sprite)            HOME reset camera
//   L link/unlink the two layer cameras (unlinked: the camera of the selected layer moves)
//   1 / 2 / 3 show/hide BG / FG / sprites (hidden layers: software mix of the captured
//   hardware indices; all visible: the real VGA pins)
//   ESC quit (press twice if there are unsaved changes)
//
// Build:  make editor
// Run:    ./obj_editor/Vppu [scenario.ppus]      (default file: scenario.ppus)
//
// File format (.ppus, little endian): "PPUSCN01", BG map, BG attr, FG map, FG attr,
// BG tiles, FG tiles, sprite tiles, BG/FG/sprite col_ram, 32 sprites (5 x int32 each:
// x, y, tile, pal, flags), 2 cameras (4 x double each: cx, cy, angle, zoom), linked flag.

#include <chrono>
#include "editor/keyboard.h"

static void draw_topbar() {
    fill_rect(0, 0, canvas_w, 30, col::PANEL);
    fill_rect(0, 30, canvas_w, 1, col::BORDER);
    tabs(8, 6, 108, MODE_NAME, NUM_MODES, current_mode);
    draw_textf(470, 11, col::TEXT, "%s%s", file_path.c_str(), unsaved ? " *" : "");
    if (!status_text[0] || SDL_GetTicks() - status_time >= STATUS_MS) return;
    bool error = strncmp(status_text, "ERROR", 5) == 0;
    draw_text(canvas_w - 8 - (int)strlen(status_text) * 8, 11, status_text, error ? col::RED : col::YELLOW);
}
static void draw_page() {
    switch (current_mode) {
        case MODE_MAP:     map_page(); break;
        case MODE_TILES:   tiles_page(); break;
        case MODE_PALETTE: palette_page(); break;
        case MODE_SPRITES: sprites_page(); break;
    }
}
// A drag goes on even outside the area where it started
static void continue_drag() {
    switch (current_drag) {
        case DRAG_PAN:
            if (mouse.left || mouse.middle)
                pan_camera((double)(mouse.x - mouse.prev_x) / VIEW_SCALE, (double)(mouse.y - mouse.prev_y) / VIEW_SCALE);
            else current_drag = DRAG_NONE;
            break;
        case DRAG_MAP:    drag_stroke(); break;
        case DRAG_TILE:   drag_pixel(); break;
        case DRAG_SPRITE: drag_sprite(); break;
    }
}
static void draw_editor() {
    std::fill(canvas.begin(), canvas.end(), col::BG);
    draw_topbar();
    count_sprites_on_lines();
    draw_ppu_output();
    view_input();
    view_controls();
    view_help();
    draw_page();
    continue_drag();
    sync_cameras();
}

static void on_mouse_button(const SDL_MouseButtonEvent& b, bool down) {
    mouse.x = b.x;
    mouse.y = b.y;
    auto set = [&](bool& held, bool& click) { held = down; click = click || down; };
    if (b.button == SDL_BUTTON_LEFT)   set(mouse.left, mouse.left_click);
    if (b.button == SDL_BUTTON_RIGHT)  set(mouse.right, mouse.right_click);
    if (b.button == SDL_BUTTON_MIDDLE) set(mouse.middle, mouse.middle_click);
}
// Returns false to quit
static bool handle_events() {
    mouse.left_click = mouse.right_click = mouse.middle_click = false;
    mouse.wheel = 0;
    mouse.prev_x = mouse.x;
    mouse.prev_y = mouse.y;
    bool running = true;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_QUIT: if (may_quit()) running = false; break;
            case SDL_KEYDOWN: if (!on_key(ev.key)) running = false; break;
            case SDL_MOUSEMOTION: mouse.x = ev.motion.x; mouse.y = ev.motion.y; break;
            case SDL_MOUSEBUTTONDOWN: on_mouse_button(ev.button, true); break;
            case SDL_MOUSEBUTTONUP: on_mouse_button(ev.button, false); break;
            case SDL_MOUSEWHEEL: mouse.wheel += ev.wheel.y; break;
            default: break;
        }
    }
    held_keys();
    sync_cameras();
    return running;
}

// Window title with the frame rate, twice per second
static void update_title(SDL_Window* window) {
    using Clock = std::chrono::steady_clock;
    static auto last = Clock::now();
    static int frames = 0;
    ++frames;
    double elapsed = std::chrono::duration<double>(Clock::now() - last).count();
    if (elapsed < 0.5) return;
    char title[256];
    snprintf(title, sizeof title, "PPU scenario editor - %s%s - %.1f fps",
             file_path.c_str(), unsaved ? " *" : "", frames / elapsed);
    SDL_SetWindowTitle(window, title);
    last = Clock::now();
    frames = 0;
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i)
        if (argv[i][0] != '+' && argv[i][0] != '-') file_path = argv[i];   // +args belong to Verilator

    sim_init(argc, argv);
    canvas_init(CANVAS_W, CANVAS_H);
    Display display;
    if (!display.open("PPU scenario editor", CANVAS_W, CANVAS_H)) return 1;
    open_scenario();

    while (handle_events() && !sim_context->gotFinish()) {
        simulate();
        draw_editor();
        display.present(canvas.data(), CANVAS_W, CANVAS_H, CANVAS_W);
        update_title(display.win);
    }

    display.close();
    sim_done();
    return 0;
}
