// frame_program.h - the program of a frame (register writes per line + OAM bands) and its execution
#pragma once
#include "graphics.h"

struct OamBand { int row; int n; uint32_t e[32]; };

struct FrameProgram {
    bool has_bg[PPU_H], has_fg[PPU_H];
    Regs bg[PPU_H], fg[PPU_H];
    std::vector<OamBand> bands;
    void clear() {
        memset(has_bg, 0, sizeof has_bg);
        memset(has_fg, 0, sizeof has_fg);
        bands.clear();
    }
    void set_bg(int L, const Regs& r) { if (L >= 0 && L < PPU_H) { has_bg[L] = true; bg[L] = r; } }
    void set_fg(int L, const Regs& r) { if (L >= 0 && L < PPU_H) { has_fg[L] = true; fg[L] = r; } }
    // OAM band for sprite rows >= row (until the next band). Returns its index.
    int band(int row) {
        for (size_t i = 0; i < bands.size(); ++i) if (bands[i].row == row) return (int)i;
        OamBand b;
        b.row = row; b.n = 0;
        std::fill(b.e, b.e + 32, OAM_HIDDEN);
        bands.push_back(b);
        return (int)bands.size() - 1;
    }
    bool push(int band_index, uint32_t e) {
        OamBand& b = bands[band_index];
        if (b.n >= 32) return false;
        b.e[b.n++] = e;
        return true;
    }
};
static FrameProgram program;
static int band_at_line[V_TOTAL];
static const Regs FG_EMPTY = Regs{0, (uint32_t)(FR_EMPTY * 8) << 16, 1u << 16, 0, 0, 0};  // m11=0: same empty row

// ---- Capture for the inspector ----
static bool capture_registers = false;
static std::vector<uint32_t> bg_pos_x(PPU_W * PPU_H), bg_pos_y(PPU_W * PPU_H), fg_pos_x(PPU_W * PPU_H), fg_pos_y(PPU_W * PPU_H);
struct LineRegs { uint32_t x0[2], y0[2], m[2][4]; uint8_t we_bg, we_fg, oam; };
static LineRegs line_regs[PPU_H];
static uint32_t scanned_oam[PPU_H][32];                          // OAM as seen by the scan of each row

// Debug probes: layer registers and position, read for the inspector
#define BG_REG(n) ppu_root->ppu__DOT__bg__DOT__u_bg_layer__DOT__##n
#define FG_REG(n) ppu_root->ppu__DOT__fg__DOT__u_bg_layer__DOT__##n

// For the inspector: map position of every pixel, registers of every line, OAM scanned for every row
static void capture_layer_state(int h, int v) {
    const bool scan_start = h == PPU_W && (v < PPU_H - 1 || v == V_TOTAL - 1);   // of the next row
    if (scan_start) memcpy(scanned_oam[v == V_TOTAL - 1 ? 0 : v + 1], ppu_oam, sizeof scanned_oam[0]);
    if (v >= PPU_H || h >= PPU_W) return;

    const int i = v * PPU_W + h;
    bg_pos_x[i] = BG_REG(x);
    bg_pos_y[i] = BG_REG(y);
    fg_pos_x[i] = FG_REG(x);
    fg_pos_y[i] = FG_REG(y);
    if (h != 0) return;
    LineRegs& l = line_regs[v];
    l.x0[L_BG] = BG_REG(x); l.y0[L_BG] = BG_REG(y);
    l.x0[L_FG] = FG_REG(x); l.y0[L_FG] = FG_REG(y);
    l.m[L_BG][0] = BG_REG(m00); l.m[L_BG][1] = BG_REG(m01); l.m[L_BG][2] = BG_REG(m10); l.m[L_BG][3] = BG_REG(m11);
    l.m[L_FG][0] = FG_REG(m00); l.m[L_FG][1] = FG_REG(m01); l.m[L_FG][2] = FG_REG(m10); l.m[L_FG][3] = FG_REG(m11);
}

// ---- Execution ----
static void rebuild_band_index() {
    for (int v = 0; v < V_TOTAL; ++v) band_at_line[v] = -1;
    for (size_t i = 0; i < program.bands.size(); ++i) {
        int row = program.bands[i].row;
        if (row < 0 || row > PPU_H - 1) continue;
        int v = row == 0 ? VBLANK_WRITE_V : row - 1;
        band_at_line[v] = (int)i;
    }
}

// One clock: bus writes scheduled for this cycle, capture, rising edge
static void step() {
    const int h = h_cnt(), v = v_cnt();

    ppu->we_scroll_bg = ppu->we_scroll_fg = 0;
    if (h == REG_WRITE_H) {
        int L = (v == VBLANK_WRITE_V) ? 0 : (v < PPU_H - 1 ? v + 1 : -1);
        if (L >= 0) {
            if (program.has_bg[L]) write_bg(program.bg[L]);
            if (program.has_fg[L]) write_fg(program.fg[L]);
            if (capture_registers) { line_regs[L].we_bg = program.has_bg[L]; line_regs[L].we_fg = program.has_fg[L]; }
        }
    }
    if (h == 0 && band_at_line[v] >= 0) {
        const OamBand& b = program.bands[band_at_line[v]];
        std::copy(b.e, b.e + 32, vram.oam);
        sync_oam();                              // 32 clocks at most, long before the scan at h = 256
        if (capture_registers && b.row < PPU_H) line_regs[b.row].oam = 1;
    }

    capture_indices(h, v);
    if (capture_registers) capture_layer_state(h, v);

    clock_ppu();
}

// One frame = [v=256,h=0 .. v=255,h=799]: vblank first (line 0 setup), then the visible lines
static void run_program() {
    sync_vram();                                 // only the first frame has VRAM to send
    rebuild_band_index();
    if (capture_registers) memset(line_regs, 0, sizeof line_regs);
    for (int i = 0; i < FRAME_CLOCKS; ++i) step();
}
