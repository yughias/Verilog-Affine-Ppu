// camera.h - moving the layer cameras, and converting between view, map and minimap positions
#pragma once
#include "widgets.h"

// ---- Camera operations: both layers when linked, otherwise only the selected one ----
static int camera_layer() {
    bool editing_layer_tiles = current_mode == MODE_TILES && tile_edit.set != L_SPR;
    return editing_layer_tiles ? tile_edit.set : brush.layer;
}
template <class F>
static void change_cameras(F change) {
    for (int layer = L_BG; layer <= L_FG; ++layer)
        if (scene.linked || layer == camera_layer()) change(scene.cam[layer]);
    unsaved = true;
}
static void sync_cameras() {
    if (scene.linked) scene.cam[L_FG] = scene.cam[L_BG];
}
static void toggle_camera_link() {
    scene.linked = !scene.linked;
    sync_cameras();
    unsaved = true;
    show_status("%s", scene.linked ? "CAMERAS LINKED" : "CAMERAS UNLINKED");
}
// Move the picture by (dx, dy) screen pixels
static void pan_camera(double dx, double dy) {
    change_cameras([&](Camera& c) {
        Xform m = cam_xform(c);
        c.cx = wrapm(c.cx - (m.a * dx + m.b * dy));
        c.cy = wrapm(c.cy - (-m.b * dx + m.a * dy));
    });
}
static void rotate_camera(double angle) {
    change_cameras([&](Camera& c) { c.ang = fmod(c.ang + angle, 2 * PI); });
}
static void zoom_camera(double factor) {
    change_cameras([&](Camera& c) { c.zoom = clampv(c.zoom * factor, 0.125, 8.0); });
}
static void center_camera(double map_x, double map_y) {
    change_cameras([&](Camera& c) { c.cx = map_x; c.cy = map_y; });
}
static void reset_camera() {
    change_cameras([](Camera& c) { c = Camera{}; });
    show_status("CAMERA RESET");
}

// ---- Mouse position in the view / minimap. Cells are not wrapped: wrap them when indexing. ----
struct Point { int x, y; };

static Point view_pixel() { return Point{fdiv(mouse.x - VIEW_X, VIEW_SCALE), fdiv(mouse.y - VIEW_Y, VIEW_SCALE)}; }
static Point view_cell(int layer) {
    Point p = view_pixel();
    double map_x, map_y;
    screen_to_map(layer, p.x, p.y, map_x, map_y);
    return Point{(int)floor(map_x / 8), (int)floor(map_y / 8)};
}
// Minimap: 2 map pixels per canvas pixel, so a cell is 4x4
static Point minimap_cell() { return Point{fdiv(mouse.x - SIDE_X, 4), fdiv(mouse.y - SIDE_Y, 4)}; }

// ---- Outlines of map areas. Corners in map pixels. ----
struct Quad { double x[4], y[4]; };

static Quad cells_quad(int x0, int y0, int x1, int y1) {   // from cell (x0, y0) to (x1, y1) included
    double left = x0 * 8, top = y0 * 8, right = (x1 + 1) * 8, bottom = (y1 + 1) * 8;
    return Quad{{left, right, right, left}, {top, top, bottom, bottom}};
}
static Quad screen_quad(int layer) {                        // what the camera of a layer shows
    Quad q = {{0, PPU_W, PPU_W, 0}, {0, 0, PPU_H, PPU_H}};
    for (int k = 0; k < 4; ++k) {
        double screen_x = q.x[k], screen_y = q.y[k];
        screen_to_map(layer, screen_x, screen_y, q.x[k], q.y[k]);
    }
    return q;
}
static void outline_on_view(int layer, Quad q, uint32_t c) {
    for (int k = 0; k < 4; ++k) {
        double screen_x, screen_y;
        map_to_screen(layer, q.x[k], q.y[k], screen_x, screen_y);
        q.x[k] = VIEW_X + screen_x * VIEW_SCALE;
        q.y[k] = VIEW_Y + screen_y * VIEW_SCALE;
    }
    for (int k = 0; k < 4; ++k) {
        int next = (k + 1) & 3;
        draw_line(q.x[k], q.y[k], q.x[next], q.y[next], c, VIEW_X, VIEW_Y, VIEW_X + VIEW_SIZE, VIEW_Y + VIEW_SIZE);
    }
}
static void outline_on_minimap(const Quad& q, uint32_t c) {
    for (int copy = 0; copy < 9; ++copy) {                  // with the wrap-around copies
        int ox = SIDE_X + (copy % 3 - 1) * SIDE_SIZE, oy = SIDE_Y + (copy / 3 - 1) * SIDE_SIZE;
        for (int k = 0; k < 4; ++k) {
            int next = (k + 1) & 3;
            draw_line(ox + q.x[k] / 2, oy + q.y[k] / 2, ox + q.x[next] / 2, oy + q.y[next] / 2, c,
                      SIDE_X, SIDE_Y, SIDE_X + SIDE_SIZE, SIDE_Y + SIDE_SIZE);
        }
    }
}
