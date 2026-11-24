// map_page.h - MAP mode: painting the BG / FG maps on the PPU view or on the minimap
#pragma once
#include "camera.h"

// ---- Cells ----
static void set_brush_cell(Point c, bool erase = false) {
    int i = cell_index(c.x, c.y);
    scene.map[brush.layer][i] = erase ? 0 : (uint8_t)brush.tile;
    scene.attr[brush.layer][i] = erase ? 0 : make_attr(brush.bank, brush.flip);
}
static void pick_cell(Point c) {
    int i = cell_index(c.x, c.y);
    uint8_t attr = scene.attr[brush.layer][i];
    brush.tile = scene.map[brush.layer][i];
    brush.bank = attr >> 4;
    brush.flip = attr & 3;
    show_status("PICKED TILE %d BANK %X FLIP %d%d", brush.tile, brush.bank, brush.flip & 1, brush.flip >> 1);
}
// Replace the connected area of cells equal to the clicked one
static void flood_fill(Point start) {
    const uint8_t* map = scene.map[brush.layer];
    const uint8_t* attr = scene.attr[brush.layer];
    const int first = cell_index(start.x, start.y);
    const uint8_t old_tile = map[first], old_attr = attr[first];
    if (old_tile == brush.tile && old_attr == make_attr(brush.bank, brush.flip)) return;

    std::vector<Point> todo{start};
    while (!todo.empty()) {
        Point c = todo.back();
        todo.pop_back();
        int i = cell_index(c.x, c.y);
        if (map[i] != old_tile || attr[i] != old_attr) continue;   // different or already painted
        set_brush_cell(c);
        const Point neighbours[4] = {{c.x + 1, c.y}, {c.x - 1, c.y}, {c.x, c.y + 1}, {c.x, c.y - 1}};
        for (const Point& n : neighbours) todo.push_back(Point{n.x & (MAP_W - 1), n.y & (MAP_H - 1)});
    }
}
// Paint all the cells between two mouse positions, so fast strokes have no holes
static void paint_segment(Point from, Point to) {
    int steps = std::max(1, std::max(abs(to.x - from.x), abs(to.y - from.y)));
    for (int i = 0; i <= steps; ++i) {
        Point c = {(int)lround(from.x + (double)(to.x - from.x) * i / steps),
                   (int)lround(from.y + (double)(to.y - from.y) * i / steps)};
        set_brush_cell(c, brush.tool == TOOL_ERASE);
    }
}

// ---- Strokes: from the mouse press to the release ----
struct Stroke {
    Point start, last;
    bool on_minimap;
};
static Stroke stroke;
static Point hovered_cell;

static void start_stroke(Point cell, bool on_minimap) {
    if (brush.tool == TOOL_PICK) { pick_cell(cell); return; }
    if (brush.tool != TOOL_RECT) push_undo();               // a rectangle is drawn at the release
    if (brush.tool == TOOL_FILL) { flood_fill(cell); return; }
    if (brush.tool != TOOL_RECT) paint_segment(cell, cell);
    stroke = Stroke{cell, cell, on_minimap};
    current_drag = DRAG_MAP;
}
static void fill_stroke_rect() {
    push_undo();
    int x0 = std::min(stroke.start.x, stroke.last.x), x1 = std::max(stroke.start.x, stroke.last.x);
    int y0 = std::min(stroke.start.y, stroke.last.y), y1 = std::max(stroke.start.y, stroke.last.y);
    for (int y = y0; y <= std::min(y1, y0 + MAP_H - 1); ++y)
        for (int x = x0; x <= std::min(x1, x0 + MAP_W - 1); ++x) set_brush_cell(Point{x, y});
}
static void drag_stroke() {
    Point cell = stroke.on_minimap ? minimap_cell() : view_cell(brush.layer);
    bool rect = brush.tool == TOOL_RECT;
    if (!rect) paint_segment(stroke.last, cell);
    stroke.last = cell;
    if (mouse.left) return;
    if (rect) fill_stroke_rect();
    current_drag = DRAG_NONE;
}
static bool dragging_rect() { return current_drag == DRAG_MAP && brush.tool == TOOL_RECT; }
static Quad stroke_quad() {
    return cells_quad(std::min(stroke.start.x, stroke.last.x), std::min(stroke.start.y, stroke.last.y),
                      std::max(stroke.start.x, stroke.last.x), std::max(stroke.start.y, stroke.last.y));
}

// Mouse over a cell of the view or of the minimap
static void map_cell_input(Point cell, bool on_minimap) {
    hovered_cell = cell;
    if (take_click(mouse.left_click, true)) start_stroke(cell, on_minimap);
    if (take_click(mouse.right_click, true)) pick_cell(cell);
}
static void map_view_input(bool over) {
    if (over) map_cell_input(view_cell(brush.layer), false);
    if (dragging_rect()) {
        if (!stroke.on_minimap) outline_on_view(brush.layer, stroke_quad(), col::YELLOW);
    } else if (over) {
        Point c = hovered_cell;
        outline_on_view(brush.layer, cells_quad(c.x, c.y, c.x, c.y), col::WHITE);
    }
}

// ---- Page ----
static void map_toolbar() {
    const int x = PAGE_X, y = TOOLBAR_Y;
    draw_text(x, y + 5, "LAYER", col::DIM);
    tabs(x + 48, y, 40, LAYER_NAME, 2, brush.layer);
    draw_text(x + 148, y + 5, "TOOL", col::DIM);
    tabs(x + 188, y, 54, TOOL_NAME, NUM_TOOLS, brush.tool, col::ORANGE);
    if (button(x + 486, y, 40, 18, "FX", (brush.flip & 1) != 0, col::PINK)) brush.flip ^= 1;
    if (button(x + 530, y, 40, 18, "FY", (brush.flip & 2) != 0, col::PINK)) brush.flip ^= 2;
    draw_text(x, BANKS_Y + 5, "BANK", col::DIM);
    brush.bank = bank_row(x + 48, BANKS_Y, 22, brush.layer, brush.bank);
}

// The whole map of the selected layer, at half scale
static void draw_minimap() {
    char title[32];
    snprintf(title, sizeof title, "%s MAP 64x64", LAYER_NAME[brush.layer]);
    panel(SIDE_X, SIDE_Y, SIDE_SIZE, SIDE_SIZE, title, col::GREEN);
    for (int v = 0; v < SIDE_SIZE; ++v)
        for (int u = 0; u < SIDE_SIZE; ++u) {
            int map_x = u * 2, map_y = v * 2, i = cell_index(map_x >> 3, map_y >> 3);
            uint8_t attr = scene.attr[brush.layer][i];
            int x = (map_x & 7) ^ ((attr & 1) ? 7 : 0), y = (map_y & 7) ^ ((attr & 2) ? 7 : 0);
            int c = tile_pixel(tile_ptr(brush.layer, scene.map[brush.layer][i]), x, y);
            bool transparent = c == 0 && has_transparency(brush.layer);
            put_pixel(SIDE_X + u, SIDE_Y + v,
                      transparent ? checker(u, v) : to_argb(scene.col[brush.layer][(attr & 0xF0) | c]));
        }
    outline_on_minimap(screen_quad(brush.layer), col::WHITE);
}
static void minimap_input() {
    bool over = mouse_in_side();
    if (over) map_cell_input(minimap_cell(), true);
    if (take_click(mouse.middle_click, over)) center_camera((mouse.x - SIDE_X) * 2.0, (mouse.y - SIDE_Y) * 2.0);
    if (dragging_rect()) outline_on_minimap(stroke_quad(), col::YELLOW);
    else if (over) draw_rect(SIDE_X + (hovered_cell.x & 63) * 4, SIDE_Y + (hovered_cell.y & 63) * 4, 4, 4, col::YELLOW);
}

static const char* flip_name(int flip) {
    static const char* const NAMES[4] = {"--", "X-", "-Y", "XY"};
    return NAMES[flip & 3];
}
static void brush_panel() {
    const int x = SIDE_X, y = SIDE_Y + 280;
    panel(x, y, SIDE_SIZE, 150, "BRUSH", col::ORANGE);
    draw_set_tile(x + 4, y + 4, 8, brush.layer, brush.tile, brush.bank, brush.flip);
    draw_rect(x + 3, y + 3, 66, 66, col::BORDER);
    draw_textf(x + 80, y + 6, col::TEXT, "TILE  %3d", brush.tile);
    draw_textf(x + 80, y + 20, col::TEXT, "BANK  %X", brush.bank);
    draw_textf(x + 80, y + 34, col::TEXT, "FLIP  %s", flip_name(brush.flip));
    draw_textf(x + 80, y + 48, col::TEXT, "TOOL  %s", TOOL_NAME[brush.tool]);
    if (mouse_in_side() || mouse_in_view()) {
        int i = cell_index(hovered_cell.x, hovered_cell.y);
        uint8_t attr = scene.attr[brush.layer][i];
        draw_textf(x + 4, y + 84, col::DIM, "CELL %2d,%2d", hovered_cell.x & 63, hovered_cell.y & 63);
        draw_textf(x + 4, y + 98, col::DIM, "TILE %3d  BANK %X  FLIP %s", scene.map[brush.layer][i], attr >> 4, flip_name(attr));
    }
    if (has_transparency(brush.layer)) draw_text(x + 4, y + 122, "FG: COLOR 0 IS TRANSPARENT", col::DIM);
}

static void map_page() {
    map_toolbar();
    brush.tile = tile_picker_panel(brush.layer, brush.bank, brush.tile);
    draw_minimap();
    minimap_input();
    brush_panel();
}
