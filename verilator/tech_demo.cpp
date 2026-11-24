// tech_demo.cpp - PPU tech demo + inspector
//
// A demo that uses every feature of the PPU (ppu.v) and, at runtime, an inspector
// (tech_demo/inspector.h) that shows what every layer is doing.
//
// THE RULE: the VRAM (BG/FG tile, map and attribute RAMs, the three col_rams and the
// sprite tile RAM) is written ONCE, in load_vram(), right after reset. Between frames
// (and inside a frame) the program only touches:
//   - the scroll/matrix registers of the two layers (ports *_bg / *_fg + we_scroll_*)
//   - the OAM (32 x 32-bit)
// Everything that moves, changes text or switches scene is done through those two.
//
// Features shown
//   BG / FG layers   : 64x64 maps (512x512 px), 256 tiles 4bpp, per-tile flip X/Y, 16 palette banks,
//                      16.16 affine matrix (scroll, rotation, zoom, shear, negative scale),
//                      wrap-around, FG transparency (color 0)
//   Raster effects   : registers reloaded per scanline (HDMA style): Mode 7 perspective,
//                      sky parallax, wave, mirror, rubber text, split screen
//   Sprites          : 32 OAM entries, 64 tiles, flip X/Y, 16 palettes, low priority
//                      (behind the FG), OAM order = sprite priority, partial clipping,
//                      per-line limit, mid-frame OAM rewrite (multiplexing: 181 sprites)
//   Text that changes: the title bar picks a different FG map row by register; numbers
//                      in the status bar are sprites
//
// Scenes (TAB / PGUP / PGDN / 1..5; they cycle automatically, A toggles autoplay)
//   1 MODE 7        per-line affine floor, FG sky with parallax, kart + billboards (lo-prio)
//   2 ROTOZOOM      whole-frame affine BG, FG colonnade, 2 sprite carousels with depth sort
//   3 RASTER FX     wave, mirror with ripple, 2x rubber text, spinning text (M00 < 0)
//   4 SPLIT SCREEN  3 viewports with independent matrices (zoom x4, rotation, shear)
//   5 SPRITE STORM  OAM rewritten 7 times per frame: 30 lanes x 6 sprites + player ship
//
// Controls
//   ESC quit      SPACE pause      N one frame (paused)      A autoplay on/off
//   TAB/PGDN next scene    PGUP previous scene    1..5 go to scene
//   V   view: PPU area 256x256  <->  whole FPGA output 640x480 (VGA)
//   I   inspector on/off
//   ARROWS  scene 1: drive the kart (autopilot comes back after 3 s)
//           scene 5: move the ship
//   Inspector: MOUSE select pixel, CLICK lock
//
// Run: ./obj_tech_demo/Vppu [--inspect] [--full] [--scene N] [--shot out.bmp FRAMES]
//
// Timing notes derived from the RTL (used by the demo and by the inspector):
//   - BG/FG pixel p: x,y used at h=p, col_idx at h=p+2, VGA pins at h=p+4
//   - sprites: the shifters start at h=794 of the previous line, so OAM X = screen x + 8
//     (X = 0..7 clips the sprite on the left; Y > 504 clips it on the top)
//   - the scan loads the first 8 sprites of a line, in OAM order
//   - a register write for line L is done at h=264 of line L-1 (line 0: in the vblank)
//   - an OAM rewrite for sprite rows >= R is done at h=0 of line R-1 (after the scan
//     of row R-1 and before the scan of row R)

#include <chrono>
#include "tech_demo/inspector.h"

using Clock = std::chrono::steady_clock;
static double seconds_between(Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double>(b - a).count(); }

struct Options {
    bool inspector = false, full_view = false;
    int scene = 0;
    const char* shot_file = nullptr;      // save a picture after shot_frames frames and quit
    long shot_frames = 0;
};
static Options parse_options(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--inspect")) o.inspector = true;
        else if (!strcmp(argv[i], "--full")) o.full_view = true;
        else if (!strcmp(argv[i], "--scene") && i + 1 < argc) o.scene = atoi(argv[++i]) - 1;
        else if (!strcmp(argv[i], "--shot") && i + 2 < argc) {
            o.shot_file = argv[++i];
            o.shot_frames = atol(argv[++i]);
        }
    }
    return o;
}

struct App {
    bool running = true, paused = false, step_once = false;
    bool inspector = false, full_view = false;
    bool captured_frame = false;          // the inspector has the data of the current frame
    bool locked = false;                  // the selected pixel does not follow the mouse
    int sel_x = 128, sel_y = 128, mouse_x = -1, mouse_y = -1;
    double sim_ms = 0, ui_fps = 0;
    int frames = 0;                       // simulated since the last fps update
    Clock::time_point last_frame = Clock::now(), last_fps = last_frame, last_title = last_frame;
};
static App app;

// ---- Input ----
static void on_key(SDL_Keycode key) {
    switch (key) {
        case SDLK_ESCAPE:   app.running = false; break;
        case SDLK_SPACE:    app.paused = !app.paused; break;
        case SDLK_n:        app.step_once = true; break;
        case SDLK_a:        demo.autoplay = !demo.autoplay; break;
        case SDLK_v:        app.full_view = !app.full_view; break;
        case SDLK_i:        app.inspector = !app.inspector; app.captured_frame = false; break;
        case SDLK_TAB:
        case SDLK_PAGEDOWN: scene_enter(demo.scene + 1); break;
        case SDLK_PAGEUP:   scene_enter(demo.scene - 1); break;
        case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4: case SDLK_5: scene_enter((int)(key - SDLK_1)); break;
        default: break;
    }
}
// Click on a pixel: lock it, or unlock it if it is the locked one
static void on_click(int mouse_x, int mouse_y) {
    int x, y;
    if (!app.inspector || !view_hit(app.full_view, mouse_x, mouse_y, x, y)) return;
    app.locked = !(app.locked && x == app.sel_x && y == app.sel_y);
    app.sel_x = x;
    app.sel_y = y;
}
static void handle_events() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) app.running = false;
        if (ev.type == SDL_KEYDOWN && !ev.key.repeat) on_key(ev.key.keysym.sym);
        if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) on_click(ev.button.x, ev.button.y);
        if (ev.type == SDL_MOUSEMOTION) {
            app.mouse_x = ev.motion.x;
            app.mouse_y = ev.motion.y;
        }
    }
    int x, y;
    if (app.inspector && !app.locked && view_hit(app.full_view, app.mouse_x, app.mouse_y, x, y)) {
        app.sel_x = x;
        app.sel_y = y;
    }
}

// ---- One frame ----
static void simulate() {
    const bool advance = !app.paused || app.step_once;
    const bool refill_capture = app.inspector && !app.captured_frame;   // paused and inspector just opened
    app.step_once = false;
    if (!advance && !refill_capture) return;

    capture_registers = app.inspector;
    if (advance) {
        build_frame(SDL_GetKeyboardState(nullptr));
        ++demo.scene_frame;
        ++demo.frame;
        if (demo.autoplay && demo.scene_frame >= SCENE_FRAMES) scene_enter(demo.scene + 1);
    }
    auto start = Clock::now();
    run_program();
    app.sim_ms = seconds_between(start, Clock::now()) * 1000;
    ++app.frames;
    if (!app.inspector) return;
    scan_sprites();
    app.captured_frame = true;
}

struct Image { const uint32_t* pixels; int w, h, stride; };
// What the window shows: the inspector, the whole monitor or the PPU area
static Image draw_image() {
    if (!app.inspector && app.full_view) return Image{vga_image.data(), VGA_W, VGA_H, VGA_W};
    if (!app.inspector) return Image{vga_image.data(), PPU_W, PPU_H, VGA_W};
    draw_inspector(InspectorInfo{app.sel_x, app.sel_y, app.locked, app.full_view, app.paused, app.sim_ms, app.ui_fps});
    return Image{canvas.data(), canvas_w, canvas_h, canvas_w};
}
static void update_rates(SDL_Window* window) {
    const auto now = Clock::now();
    const double frame_time = seconds_between(app.last_frame, now), elapsed = seconds_between(app.last_fps, now);
    app.last_frame = now;
    if (frame_time > 0) app.ui_fps = app.ui_fps * 0.9 + 0.1 / frame_time;
    if (elapsed >= 0.5) {
        demo.sim_fps = app.frames / elapsed;
        app.frames = 0;
        app.last_fps = now;
    }
    if (seconds_between(app.last_title, now) <= 0.5) return;
    app.last_title = now;
    char title[160];
    snprintf(title, sizeof title, "PPU tech demo - %d/5 %s - %.1f fps - %s view [V]  inspector [I]  %s",
             demo.scene + 1, SCENE_NAME[demo.scene], demo.sim_fps, app.full_view ? "640x480" : "256x256",
             app.paused ? "PAUSED" : demo.autoplay ? "autoplay [A]" : "manual [A]");
    SDL_SetWindowTitle(window, title);
}
static void save_shot(const Image& img, const char* file) {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom((void*)img.pixels, img.w, img.h, 32, img.stride * 4,
                                                              SDL_PIXELFORMAT_XRGB8888);
    SDL_SaveBMP(surface, file);
    SDL_FreeSurface(surface);
    printf("frame %ld scene %d  sim %.1f ms -> %s\n", demo.frame, demo.scene + 1, app.sim_ms, file);
}

int main(int argc, char** argv) {
    const Options options = parse_options(argc, argv);
    app.inspector = options.inspector;
    app.full_view = options.full_view;

    sim_init(argc, argv);
    canvas_init(1600, 900);
    Display display;
    if (!display.open("PPU tech demo", canvas_w, canvas_h)) return 1;
    load_vram();
    place_road_objects();
    scene_enter(options.scene);
    program.clear();

    while (app.running && !sim_context->gotFinish()) {
        handle_events();
        simulate();
        const Image img = draw_image();
        display.present(img.pixels, img.w, img.h, img.stride, app.inspector);
        update_rates(display.win);
        if (options.shot_file && demo.frame >= options.shot_frames) {
            save_shot(img, options.shot_file);
            app.running = false;
        }
    }

    display.close();
    sim_done();
    return 0;
}
