// scenes.h - the five scenes: every frame each of them writes registers and OAM, nothing else
#pragma once
#include "frame_program.h"

constexpr int SCENE_FRAMES = 600;         // autoplay: frames per scene
constexpr int MANUAL_FRAMES = 180;        // the autopilot comes back after these frames without keys

static inline double wrap256(double v) { v = fmod(v, 256.0); return v < 0 ? v + 256.0 : v; }
static inline double wrapdm(double v) { v = wrapm(v); return v >= MAP_PX / 2 ? v - MAP_PX : v; }   // signed, world coords

struct Arrows {
    bool left, right, up, down;
    bool any() const { return left || right || up || down; }
};

struct DemoState {
    int scene = 0, scene_frame = 0;
    long frame = 0;
    bool autoplay = true;
    // scene 1: kart
    double kart_x = 108, kart_y = 68, angle = 0, speed = 1.6;
    int waypoint = 0, manual_frames = 0;
    // scene 3: bouncing sprites
    double ball_x[6], ball_y[6], ball_vx[6], ball_vy[6], ball_jump[6];
    // scene 5: ship
    double ship_x = 124, ship_y = 200;
    int ship_manual_frames = 0;
    // status bar
    int sprites_shown = 0;
    double sim_fps = 0;
};
static DemoState demo;

// Autopilot waypoints = track corners in pixels; waypoint i is the end of segment i
static inline double waypoint_x(int i) { return TRACK[(i + 1) % TRACK_N][0] * 8 + 4; }
static inline double waypoint_y(int i) { return TRACK[(i + 1) % TRACK_N][1] * 8 + 4; }
struct WorldObj { double x, y; int kind; };                                    // 0 coin, 1 cone
static std::vector<WorldObj> road_objects;

static void place_road_objects() {   // C++-side only: positions of the billboards
    road_objects.clear();
    const double finish_px = FINISH_TX * 8 + 4;
    for (int i = 0; i < TRACK_N; ++i) {
        int j = (i + 1) % TRACK_N;
        double ax = TRACK[i][0] * 8 + 4, ay = TRACK[i][1] * 8 + 4;
        double bx = TRACK[j][0] * 8 + 4, by = TRACK[j][1] * 8 + 4;
        double len = hypot(bx - ax, by - ay);
        for (double s = 24; s < len - 24; s += 32) {            // coins along the centreline
            double x = ax + (bx - ax) * s / len, y = ay + (by - ay) * s / len;
            if (i == 0 && fabs(x - finish_px) < 16) continue;   // keep the finish line clear
            road_objects.push_back({x, y, 0});
        }
    }
    for (int i = 0; i < TRACK_N; ++i) {                         // a cone outside every corner
        int p = (i + TRACK_N - 1) % TRACK_N, n = (i + 1) % TRACK_N;
        double cx = TRACK[i][0] * 8 + 4, cy = TRACK[i][1] * 8 + 4;
        double d1x = TRACK[i][0] - TRACK[p][0], d1y = TRACK[i][1] - TRACK[p][1];
        double d2x = TRACK[n][0] - TRACK[i][0], d2y = TRACK[n][1] - TRACK[i][1];
        double l1 = hypot(d1x, d1y), l2 = hypot(d2x, d2y);
        double ox = d1x / l1 - d2x / l2, oy = d1y / l1 - d2y / l2, lo = hypot(ox, oy);
        road_objects.push_back({cx + 30 * ox / lo, cy + 30 * oy / lo, 1});
    }
}

static void scene_enter(int s) {
    demo.scene = (s % NUM_SCENES + NUM_SCENES) % NUM_SCENES;
    demo.scene_frame = 0;
    if (demo.scene == 0) {   // start on the top straight, heading east
        demo.kart_x = TRACK[0][0] * 8 + 4 + 40; demo.kart_y = TRACK[0][1] * 8 + 4;
        demo.angle = 0; demo.speed = 1.6; demo.waypoint = 0; demo.manual_frames = 0;
    }
    if (demo.scene == 2)
        for (int i = 0; i < 6; ++i) {
            demo.ball_x[i] = 20 + i * 38; demo.ball_y[i] = 60 + i * 22;
            demo.ball_vx[i] = ((i & 1) ? -1 : 1) * (0.8 + 0.25 * i); demo.ball_vy[i] = 0;
            demo.ball_jump[i] = 6.5 + 0.25 * i;
        }
    if (demo.scene == 4) { demo.ship_x = 124; demo.ship_y = 200; demo.ship_manual_frames = 0; }
}

// ---- Scene 1: MODE 7 ----------------------------------------------------------
constexpr double CAMERA_HEIGHT = 40, FOCAL = 120;
constexpr int HORIZON = 72;                    // the FG sky covers lines 8..71

struct Heading { double fwd_x, fwd_y, right_x, right_y; };
static Heading kart_heading() {
    double c = cos(demo.angle), s = sin(demo.angle);
    return Heading{c, s, -s, c};
}
// Vector from the kart to a waypoint, on the shortest way around the world
static double waypoint_dx(int i) { return wrapdm(waypoint_x(i) - demo.kart_x); }
static double waypoint_dy(int i) { return wrapdm(waypoint_y(i) - demo.kart_y); }
static double angle_to_waypoint(int i) {
    return remainder(atan2(waypoint_dy(i), waypoint_dx(i)) - demo.angle, 2 * PI);
}
static bool waypoint_within(int i, double distance) {
    return hypot(waypoint_dx(i), waypoint_dy(i)) < distance;
}

static void steer_by_keys(const Arrows& arrows) {
    if (arrows.left) demo.angle -= 0.045;
    if (arrows.right) demo.angle += 0.045;
    if (arrows.up) demo.speed = std::min(3.2, demo.speed + 0.06);
    if (arrows.down) demo.speed = std::max(0.0, demo.speed - 0.1);
}
// Waypoint closest to the heading: where the autopilot goes when it takes over
static int waypoint_ahead() {
    int best = demo.waypoint;
    double best_angle = 1e9;
    for (int i = 0; i < TRACK_N; ++i) {
        double angle = fabs(angle_to_waypoint(i));
        if (waypoint_within(i, 30) || angle >= best_angle) continue;
        best_angle = angle;
        best = i;
    }
    return best;
}
static void steer_by_autopilot() {
    if (waypoint_within(demo.waypoint, 26)) demo.waypoint = (demo.waypoint + 1) % TRACK_N;
    demo.angle += std::max(-0.05, std::min(0.05, angle_to_waypoint(demo.waypoint)));
    demo.speed += (1.8 - demo.speed) * 0.05;
}
static void drive_kart(const Arrows& arrows) {
    if (arrows.any()) demo.manual_frames = MANUAL_FRAMES;
    if (demo.manual_frames > 0) {
        steer_by_keys(arrows);
        if (--demo.manual_frames == 0) demo.waypoint = waypoint_ahead();
    } else {
        steer_by_autopilot();
    }
    demo.kart_x = wrapm(demo.kart_x + cos(demo.angle) * demo.speed);
    demo.kart_y = wrapm(demo.kart_y + sin(demo.angle) * demo.speed);
}

// BG: one matrix per scanline (the HDMA part)
static void mode7_floor(const Heading& d) {
    for (int line = HORIZON; line < 248; ++line) {
        double below = line - HORIZON + 1, distance = CAMERA_HEIGHT * FOCAL / below, step = CAMERA_HEIGHT / below;
        double x = demo.kart_x + d.fwd_x * distance - d.right_x * step * 128;
        double y = demo.kart_y + d.fwd_y * distance - d.right_y * step * 128;
        program.set_bg(line, regs_mat(x, y, d.right_x * step, d.right_y * step, 0, 0));
    }
}
// FG: sky panorama, 360 degrees = 256 px, clouds drift
static void mode7_sky() {
    double pan = demo.angle * 128.0 / PI;
    program.set_fg(8, regs_ident(wrap256(pan + demo.scene_frame * 0.25), FR_SKY * 8));
    program.set_fg(40, regs_ident(wrap256(pan), FR_SKY * 8 + 32));
    program.set_fg(HORIZON, FG_EMPTY);
}

// Keeps every line within the number of sprites the hardware can show
struct LineBudget {
    int count[PPU_H] = {0};
    bool fits(int y) const {
        for (int line = std::max(0, y); line < std::min(PPU_H, y + 8); ++line)
            if (count[line] >= SPR_LINE_LIMIT) return false;
        return true;
    }
    void add(int y) {
        for (int line = std::max(0, y); line < std::min(PPU_H, y + 8); ++line) ++count[line];
    }
};
// 16x16 = 4 sprites, the right half is the left one flipped. Returns the sprites used.
static int mode7_kart(int band, LineBudget& lines) {
    bool bump = demo.speed > 0.5 && ((demo.frame / 4) & 1);
    for (int k = 0; k < 4; ++k) {
        int x = 120 + (k & 1) * 8, y = 200 + (bump ? 1 : 0) + (k >> 1) * 8;
        program.push(band, oam_entry(x, y, (k >> 1) ? ST_KART_BL : ST_KART_TL, SP_KART, (k & 1) != 0));
        lines.add(y);
    }
    return 4;
}

struct Billboard { double distance; int x, y, kind, id; };
// Objects in front of the kart, the nearest first
static std::vector<Billboard> visible_billboards(const Heading& d) {
    std::vector<Billboard> out;
    for (size_t i = 0; i < road_objects.size(); ++i)
        for (int copy = 0; copy < 9; ++copy) {           // the world wraps around
            double dx = road_objects[i].x + (copy % 3 - 1) * MAP_PX - demo.kart_x;
            double dy = road_objects[i].y + (copy / 3 - 1) * MAP_PX - demo.kart_y;
            double ahead = dx * d.fwd_x + dy * d.fwd_y, side = dx * d.right_x + dy * d.right_y;
            if (ahead < 28 || ahead > 320) continue;
            int x = (int)lround(128 + side * FOCAL / ahead) - 4;
            int y = (int)lround(HORIZON - 1 + CAMERA_HEIGHT * FOCAL / ahead) - 7;
            if (x < -8 || x > 247 || y > 240) continue;
            out.push_back({ahead, x, y, road_objects[i].kind, (int)i});
        }
    std::sort(out.begin(), out.end(), [](const Billboard& a, const Billboard& b) { return a.distance < b.distance; });
    return out;
}
// Low priority: behind the opaque FG sky, so billboards near the horizon get clipped
static uint32_t billboard_oam(const Billboard& b) {
    if (b.kind != 0) return oam_entry(b.x, b.y, ST_CONE, SP_CONE, false, false, true);
    static const int COIN_TILES[4] = {ST_COIN0, ST_COIN1, ST_COIN2, ST_COIN1};
    int step = (int)((demo.frame / 5 + b.id) & 3);
    return oam_entry(b.x, b.y, COIN_TILES[step], SP_GOLD, step == 3, false, true);
}
static void scene_mode7(const Arrows& arrows) {
    drive_kart(arrows);
    const Heading heading = kart_heading();
    mode7_floor(heading);
    mode7_sky();

    LineBudget lines;
    const int band = program.band(0);
    int used = mode7_kart(band, lines);
    for (const Billboard& b : visible_billboards(heading)) {   // nearest first = higher OAM priority
        if (used >= NUM_OAM) break;
        if (!lines.fits(b.y)) continue;
        lines.add(b.y);
        program.push(band, billboard_oam(b));
        ++used;
    }
    demo.sprites_shown = used;
}

// ---- Scene 2: ROTOZOOM + PRIORITY ---------------------------------------------
static void scene_rotozoom() {
    double t = demo.scene_frame / 60.0;
    // rotates around the logo / inner lake area of the 512x512 world
    program.set_bg(8, regs_rotzoom(8, 230 + 80 * sin(t * 0.3), 140 + 30 * sin(t * 0.23), t * 0.6, 1.0 + 0.55 * sin(t * 0.9)));
    double px = 30 * sin(t * 0.7) - t * 12;
    for (int L = 8; L < 248; L += 64) program.set_fg(L, regs_ident(px, FR_PILLARS * 8));  // colonnade band repeated

    struct Spr { double depth; uint32_t e; };
    std::vector<Spr> v;
    for (int k = 0; k < 2; ++k) {
        double cx = 128, cy = k ? 172 : 100, rxx = k ? 92 : 104, ryy = 26, dir = k ? -1.0 : 1.0;
        for (int i = 0; i < 16; ++i) {
            double th = dir * t * 1.1 + i * 2 * PI / 16;
            double s = sin(th), c = cos(th);
            int x = (int)lround(cx + rxx * c) - 4, y = (int)lround(cy + ryy * s) - 4;
            bool moving_left = (-rxx * s * dir) < 0;
            int tile = ST_SMILEY + (i % 8), pal = SP_YELLOW + ((i / 8) + 2 * k) % 4;
            bool fy = (i % 8 == 7) && ((demo.scene_frame / 40) & 1);
            v.push_back({s, oam_entry(x, y, tile, pal, moving_left, fy, s < 0)});  // back half behind the FG
        }
    }
    std::sort(v.begin(), v.end(), [](const Spr& a, const Spr& b) { return a.depth > b.depth; });  // front = OAM first
    int b0 = program.band(0);
    for (auto& s : v) program.push(b0, s.e);
    demo.sprites_shown = (int)v.size();
}

// ---- Scene 3: RASTER FX -------------------------------------------------------
static void scene_raster() {
    double t = demo.scene_frame / 60.0;
    double bxs = 96 + 60 * sin(t * 0.25), bys = 88;     // logo + inner lake area
    for (int L = 8; L < 136; ++L) {                     // wave
        int k = L - 8;
        program.set_bg(L, regs_mat(bxs + 5 * sin(k * 0.12 + t * 2.5), bys + k, 1, 0, 0, 1));
    }
    for (int L = 136; L < 248; ++L) {                   // mirror (reads the map upwards) + ripple
        int k = L - 136;
        double amp = 1.5 + k * 0.06;
        program.set_bg(L, regs_mat(bxs + amp * sin(k * 0.5 - t * 5), bys + 127 - k * 1.15 + 1.2 * sin(k * 0.4 + t * 4),
                                1, 0, 0, -1));
    }
    double sc = t * 40;                                 // rubber text, zoom x2
    for (int L = 40; L < 56; ++L) {
        // one offset per texel row (= 2 screen lines) and a small amplitude, so the text stays readable
        int tr = (L - 40) / 2;
        program.set_fg(L, regs_mat(sc + 1.5 * sin(tr * 0.7 + t * 3), FR_TEXT_A * 8 + tr, 0.5, 0, 0, 0.5));
    }
    program.set_fg(56, FG_EMPTY);
    double c = cos(t * 1.8);                            // spinning text: m00 = 0.5/cos, negative = mirrored
    if (fabs(c) < 0.2) program.set_fg(160, FG_EMPTY);
    else {
        double m00 = 0.5 / c;
        program.set_fg(160, regs_mat(64 - 128 * m00, FR_TEXT_B * 8, m00, 0, 0, 0.5));   // one write, 16 lines
    }
    program.set_fg(176, FG_EMPTY);

    static const int TL[6] = {ST_ORB, ST_STAR, ST_HEART, ST_SMILEY, ST_GHOST, ST_DIAMOND};
    static const int PL[6] = {SP_CYAN, SP_YELLOW, SP_RED, SP_GREEN, SP_PURPLE, SP_ORANGE};
    int b0 = program.band(0);
    for (int i = 0; i < 6; ++i) {
        demo.ball_vy[i] += 0.15;
        demo.ball_x[i] += demo.ball_vx[i];
        demo.ball_y[i] += demo.ball_vy[i];
        if (demo.ball_y[i] > 232) { demo.ball_y[i] = 232; demo.ball_vy[i] = -demo.ball_jump[i]; }
        if (demo.ball_x[i] < 0)   { demo.ball_x[i] = 0;   demo.ball_vx[i] = fabs(demo.ball_vx[i]); }
        if (demo.ball_x[i] > 247) { demo.ball_x[i] = 247; demo.ball_vx[i] = -fabs(demo.ball_vx[i]); }
        int y = std::max(8, (int)lround(demo.ball_y[i]));
        program.push(b0, oam_entry((int)lround(demo.ball_x[i]), y, TL[i], PL[i], demo.ball_vx[i] < 0, false, (i & 1) != 0));
    }
    demo.sprites_shown = 6;
}

// ---- Scene 4: SPLIT SCREEN ----------------------------------------------------
static void scene_split() {
    double t = demo.scene_frame / 60.0;
    // view 1 (lines 8..87): zoom x4 sweeping over the logo and the inner lake (flips + palette banks)
    program.set_bg(8, regs_rotzoom(8, 230 + 100 * sin(t * 0.4), 136 + 16 * sin(t * 0.53), 0, 4.0, 128, 48));
    // view 2 (lines 88..167): rotation around the finish line
    program.set_bg(88, regs_rotzoom(88, FINISH_TX * 8 + 4, TRACK[0][1] * 8 + 4, -t * 0.8, 1.3, 128, 128));
    // view 3 (lines 168..247): shear (m10 = x shift per line) + vertical stretch (m11 = 0.5), forest + big lake
    program.set_bg(168, regs_mat(t * 50, 380, 1, 0, 0.6 * sin(t * 1.3), 0.5));
    for (int k = 0; k < 3; ++k) {                       // full-width label bars = dividers
        program.set_fg(8 + 80 * k, regs_ident(0, (FR_LABEL + k) * 8));
        program.set_fg(16 + 80 * k, FG_EMPTY);
    }
    int b0 = program.band(0);
    for (int i = 0; i < 8; ++i) {                       // ring counter-rotating against view 2
        double a = t * 2 + i * PI / 4;
        double c = cos(a), s = sin(a);
        int x = (int)lround(128 + 28 * c) - 4, y = (int)lround(128 + 28 * s) - 4;
        program.push(b0, oam_entry(x, y, ST_ARROW, SP_YELLOW + (i & 3), c < 0, s < 0));
    }
    for (int i = 0; i < 7; ++i) {                       // hearts marching in view 3
        int x = (int)wrap256(i * 36 + t * 60) - 8;
        int y = (int)lround(210 + 10 * sin(t * 4 + i));
        program.push(b0, oam_entry(x, y, ST_HEART, (i & 1) ? SP_RED : SP_PURPLE));
    }
    demo.sprites_shown = 15;
}

// ---- Scene 5: SPRITE STORM (multiplexing) ---------------------------------------
static void scene_storm(const Arrows& arrows) {
    double t = demo.scene_frame / 60.0;
    program.set_bg(8, regs_rotzoom(8, 256 + 60 * sin(t * 0.2), -t * 30, 0.15 * sin(t * 0.5), 1.25));
    program.set_fg(112, regs_mat(t * 20, FR_STORM * 8, 0.5, 0, 0, 0.5));
    program.set_fg(128, FG_EMPTY);

    if (arrows.any()) demo.ship_manual_frames = MANUAL_FRAMES;
    if (demo.ship_manual_frames > 0) {
        --demo.ship_manual_frames;
        if (arrows.left) demo.ship_x -= 2.5;
        if (arrows.right) demo.ship_x += 2.5;
        if (arrows.up) demo.ship_y -= 2.5;
        if (arrows.down) demo.ship_y += 2.5;
    } else {
        demo.ship_x += (124 + 100 * sin(t * 0.9) - demo.ship_x) * 0.1;
        demo.ship_y += (128 + 90 * sin(t * 1.3) - demo.ship_y) * 0.1;
    }
    demo.ship_x = std::max(0.0, std::min(247.0, demo.ship_x));
    demo.ship_y = std::max(8.0, std::min(239.0, demo.ship_y));
    int sx = (int)lround(demo.ship_x), sy = (int)lround(demo.ship_y);
    uint32_t ship = oam_entry(sx, sy, ST_SHIP, SP_SHIP);

    static const int TL[10] = {ST_SPARK, ST_STAR, ST_DIAMOND, ST_HEART, ST_ORB,
                               ST_GHOST, ST_SMILEY, ST_MUSHROOM, ST_ARROW, ST_COIN0};
    static const int PL[8] = {SP_YELLOW, SP_RED, SP_BLUE, SP_GREEN, SP_PURPLE, SP_CYAN, SP_ORANGE, SP_GOLD};
    // 30 lanes of 8 lines (y = 8..247), 6 sprites each; 6 bands of 5 lanes, each with its own OAM
    for (int b = 0; b < 6; ++b) {
        int row = b == 0 ? 0 : 8 + 40 * b, end = b == 5 ? 248 : 8 + 40 * (b + 1);
        int bi = program.band(row);
        if (sy + 7 >= row && sy < end) program.push(bi, ship);   // a ship crossing a band goes in both
        for (int j = 5 * b; j < 5 * b + 5; ++j) {
            double sp = (0.6 + ((j * 7) % 5) * 0.5) * ((j & 1) ? -1 : 1);
            double off = sp * demo.scene_frame + j * 37;
            int tile = TL[j % 10], pal = PL[j % 8];
            bool spin = tile == ST_STAR || tile == ST_DIAMOND || tile == ST_SPARK;
            for (int k = 0; k < 6; ++k) {
                int x = (int)wrap256(off + k * 256.0 / 6) - 8;
                bool fx = spin ? (((demo.scene_frame / 8) + k) & 1) != 0 : sp < 0;
                program.push(bi, oam_entry(x, 8 + 8 * j, tile, pal, fx, false, j % 3 == 1));
            }
        }
    }
    demo.sprites_shown = 181;
}

// Digits are sprites in their own OAM band (rows 248..255)
static void draw_status_bar() {
    int fps = std::max(0, std::min(99, (int)lround(demo.sim_fps)));
    int n = std::max(0, std::min(999, demo.sprites_shown));
    int hb = program.band(248);
    program.push(hb, oam_entry(5 * 8, 248, ST_DIGIT0 + fps / 10, SP_HUD));
    program.push(hb, oam_entry(6 * 8, 248, ST_DIGIT0 + fps % 10, SP_HUD));
    program.push(hb, oam_entry(17 * 8, 248, ST_DIGIT0 + n / 100, SP_HUD));
    program.push(hb, oam_entry(18 * 8, 248, ST_DIGIT0 + (n / 10) % 10, SP_HUD));
    program.push(hb, oam_entry(19 * 8, 248, ST_DIGIT0 + n % 10, SP_HUD));
}

// Builds the whole program of the next frame (registers + OAM bands)
static void build_frame(const Uint8* keys) {
    const Arrows arrows = {keys[SDL_SCANCODE_LEFT] != 0, keys[SDL_SCANCODE_RIGHT] != 0,
                           keys[SDL_SCANCODE_UP] != 0, keys[SDL_SCANCODE_DOWN] != 0};
    program.clear();
    program.set_fg(0, regs_ident(0, (FR_TITLE + demo.scene) * 8));   // title = FG row picked by register
    program.set_fg(8, FG_EMPTY);
    program.set_fg(248, regs_ident(0, FR_STATUS * 8));
    program.band(0);                                               // every frame starts from a clean OAM
    switch (demo.scene) {
        case 0: scene_mode7(arrows); break;
        case 1: scene_rotozoom(); break;
        case 2: scene_raster(); break;
        case 3: scene_split(); break;
        default: scene_storm(arrows); break;
    }
    draw_status_bar();
}
