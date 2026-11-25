module bg_layer_top #(
    parameter SCREEN_W       = 256,
    parameter SCREEN_H       = 256,
    parameter TILE_SIZE      = 8,
    parameter DATA_WIDTH     = 8,
    parameter TILE_IDX_WIDTH = 8,

    parameter MAP_ADDR_WIDTH  = $clog2(4 * (SCREEN_W / TILE_SIZE) * (SCREEN_H / TILE_SIZE)),
    parameter TILE_ADDR_WIDTH = 13
) (
    input wire clk,
    input wire rst,

    input wire hblank,
    input wire vblank,

    output wire [7:0] col_idx,

    input wire [31:0] scroll_x,
    input wire [31:0] scroll_y,
    input wire [31:0] m00,
    input wire [31:0] m01,
    input wire [31:0] m10,
    input wire [31:0] m11,
    input wire we_scroll,

    input wire vram_we,
    input wire [13:0] vram_addr,
    input wire [7:0] vram_din
);
    wire [MAP_ADDR_WIDTH-1:0]  map_addr;
    wire [TILE_IDX_WIDTH-1:0]  map_dout;
    wire [TILE_ADDR_WIDTH-1:0] tile_addr;
    wire [DATA_WIDTH-1:0]      tile_dout;
    wire [MAP_ADDR_WIDTH-1:0]  attr_addr;
    wire [TILE_IDX_WIDTH-1:0]  attr_dout;

    wire tile_ram_we = vram_we && !vram_addr[13];
    wire map_ram_we = vram_we && (vram_addr[13:12] == 2'b10);
    wire attr_ram_we = vram_we && (vram_addr[13:12] == 2'b11);

    bg_layer #(
        .TILE_SIZE       (TILE_SIZE),
        .TILE_ADDR_WIDTH (TILE_ADDR_WIDTH),
        .MAP_ADDR_WIDTH  (MAP_ADDR_WIDTH),
        .DATA_WIDTH      (DATA_WIDTH),
        .TILE_IDX_WIDTH  (TILE_IDX_WIDTH)
    ) u_bg_layer (
        .clk         (clk),
        .rst         (rst),
        .hblank      (hblank),
        .vblank      (vblank),
        .map_addr    (map_addr),
        .map_dout    (map_dout),
        .tile_addr   (tile_addr),
        .tile_dout   (tile_dout),
        .attr_addr   (attr_addr),
        .attr_dout   (attr_dout),
        .col_idx   (col_idx),
        .in_scroll_x (scroll_x),
        .in_scroll_y (scroll_y),
        .in_m00      (m00),
        .in_m01      (m01),
        .in_m10      (m10),
        .in_m11      (m11),
        .we_scroll   (we_scroll)
    );

    // Tile RAM: 256 tiles * 32 bytes/tile = 8192 bytes (ADDR_WIDTH = 13)
    ram #(
        .DATA_WIDTH(DATA_WIDTH),
        .ADDR_WIDTH(TILE_ADDR_WIDTH)
    ) tile_ram (
        .clk  (clk),
        .we   (tile_ram_we),
        .addr_w (vram_addr[12:0]),
        .addr_r (tile_addr),
        .din  (vram_din),
        .dout (tile_dout)
    );

    // Map RAM: 64x64 tiles = 4096 entries (ADDR_WIDTH = 12)
    ram #(
        .DATA_WIDTH(TILE_IDX_WIDTH),
        .ADDR_WIDTH(MAP_ADDR_WIDTH)
    ) map_ram (
        .clk  (clk),
        .we   (map_ram_we),
        .addr_w (vram_addr[11:0]),
        .addr_r (map_addr),
        .din  (vram_din),
        .dout (map_dout)
    );

    ram #(
        .DATA_WIDTH(DATA_WIDTH),
        .ADDR_WIDTH(MAP_ADDR_WIDTH)
    ) attr_ram (
        .clk  (clk),
        .we   (attr_ram_we),
        .addr_w (vram_addr[11:0]),
        .addr_r (attr_addr),
        .din  (vram_din),
        .dout (attr_dout)
    );
endmodule
