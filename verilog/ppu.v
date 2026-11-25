module ppu (
    input  wire       clk,
    input  wire       rst,

    // VGA Physical Pins
    output wire       hsync,
    output wire       vsync,
    output wire [7:0] vga_r,
    output wire [7:0] vga_g,
    output wire [7:0] vga_b,

    input wire        we_scroll_fg,
    input wire [31:0]  scroll_x_fg,
    input wire [31:0]  scroll_y_fg,
    input wire [31:0]  m00_fg,
    input wire [31:0]  m01_fg,
    input wire [31:0]  m10_fg,
    input wire [31:0]  m11_fg,

    input wire        we_scroll_bg,
    input wire [31:0]  scroll_x_bg,
    input wire [31:0]  scroll_y_bg,
    input wire [31:0]  m00_bg,
    input wire [31:0]  m01_bg,
    input wire [31:0]  m10_bg,
    input wire [31:0]  m11_bg,

    input wire we_vram,
    input wire [15:0] vram_addr_in,
    input wire [7:0] vram_din,

    input wire we_oam,
    input wire [4:0] oam_addr_in,
    input wire [31:0] oam_din
);
    localparam PPU_WIDTH = 256;
    localparam PPU_HEIGHT = 256;

    wire [9:0] vga_h_cnt;
    wire [9:0] vga_v_cnt;
    wire vga_hsync;
    wire vga_vsync;

    wire ppu_hblank = vga_h_cnt >= PPU_WIDTH;
    wire ppu_vblank = vga_v_cnt >= PPU_HEIGHT;
    wire blank;

    wire [2:0] r;
    wire [2:0] g;
    wire [1:0] b;
    assign vga_r = blank ? 0 : {r, r, r[1:0]};
    assign vga_g = blank ? 0 : {g, g, g[1:0]};
    assign vga_b = blank ? 0 : {b, b, b, b};

    wire [7:0] fg_idx;
    wire [7:0] bg_idx;
    wire [7:0] sprite_idx;
    wire sprite_lo_prio;

    // chip select
    wire [1:0] vram_chip_sel = vram_addr_in[15:14];
    wire [13:0] vram_addr = vram_addr_in[13:0];
    wire bg_we = we_vram && (vram_chip_sel == 2'b00);
    wire fg_we = we_vram && (vram_chip_sel == 2'b01);
    wire col_we = we_vram && (vram_chip_sel == 2'b10);
    wire tile_sprite_we = we_vram && (vram_chip_sel == 2'b11);

    vga u_vga (
        .clk_25mhz   (clk),
        .rst         (rst),
        .hsync       (vga_hsync),
        .vsync       (vga_vsync),
        .h_cnt       (vga_h_cnt),
        .v_cnt       (vga_v_cnt)
    );

    pipe #(
        .N (4)
    ) hsync_pipe (
        .clk  (clk),
        .rst  (rst),
        .en   (1),
        .din  (vga_hsync),
        .dout (hsync)
    );

    pipe #(
        .N (4)
    ) vsync_pipe (
        .clk  (clk),
        .rst  (rst),
        .en   (1),
        .din  (vga_vsync),
        .dout (vsync)
    );

    pipe #(
        .N (4)
    ) blank_pipe (
        .clk  (clk),
        .rst  (rst),
        .en   (1),
        .din  (ppu_vblank || ppu_hblank),
        .dout (blank)
    );

    bg_layer_top #() fg (
        .clk (clk),
        .rst (rst),
        .hblank (ppu_hblank),
        .vblank (ppu_vblank),
        .col_idx (fg_idx),

        .scroll_x (scroll_x_fg),
        .scroll_y (scroll_y_fg),
        .m00 (m00_fg),
        .m01 (m01_fg),
        .m10 (m10_fg),
        .m11 (m11_fg),
        .we_scroll (we_scroll_fg),

        .vram_we (fg_we),
        .vram_addr (vram_addr),
        .vram_din (vram_din)
    );

    bg_layer_top #() bg (
        .clk (clk),
        .rst (rst),
        .hblank (ppu_hblank),
        .vblank (ppu_vblank),
        .col_idx (bg_idx),

        .scroll_x (scroll_x_bg),
        .scroll_y (scroll_y_bg),
        .m00 (m00_bg),
        .m01 (m01_bg),
        .m10 (m10_bg),
        .m11 (m11_bg),
        .we_scroll (we_scroll_bg),

        .vram_we (bg_we),
        .vram_addr (vram_addr),
        .vram_din (vram_din)
    );

    sprite_layer_top #() u_sprite_layer (
        .clk (clk),
        .rst (rst),
        .h_cnt (vga_h_cnt),
        .v_cnt (vga_v_cnt),

        .col_out (sprite_idx),
        .lo_prio_out (sprite_lo_prio),

        .oam_we (we_oam),
        .oam_addr_in (oam_addr_in),
        .oam_din (oam_din),

        .tile_we (tile_sprite_we),
        .tile_addr_in (vram_addr[12:0]),
        .tile_din (vram_din)
    );

    color_mixer #() mixer (
        .clk (clk),
        .rst (rst),

        .fg_idx (fg_idx),
        .bg_idx (bg_idx),
        .sprite_idx (sprite_idx),
        .sprite_lo_prio_in (sprite_lo_prio),

        .r (r),
        .g (g),
        .b (b),

        .we (col_we),
        .vram_addr (vram_addr),
        .vram_din (vram_din)
    );

    
endmodule
