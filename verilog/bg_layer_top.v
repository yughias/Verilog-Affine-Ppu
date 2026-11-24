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
    input wire we_scroll
);
    wire [MAP_ADDR_WIDTH-1:0]  map_addr;
    wire [TILE_IDX_WIDTH-1:0]  map_dout;
    wire [TILE_ADDR_WIDTH-1:0] tile_addr;
    wire [DATA_WIDTH-1:0]      tile_dout;
    wire [MAP_ADDR_WIDTH-1:0]  attr_addr;
    wire [TILE_IDX_WIDTH-1:0]  attr_dout;

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
        .we   (1'b0),
        .addr (tile_addr),
        .din  ({DATA_WIDTH{1'b0}}),
        .dout (tile_dout)
    );

    // Map RAM: 64x64 tiles = 4096 entries (ADDR_WIDTH = 12)
    ram #(
        .DATA_WIDTH(TILE_IDX_WIDTH),
        .ADDR_WIDTH(MAP_ADDR_WIDTH)
    ) map_ram (
        .clk  (clk),
        .we   (1'b0),
        .addr (map_addr),
        .din  ({TILE_IDX_WIDTH{1'b0}}),
        .dout (map_dout)
    );

    ram #(
        .DATA_WIDTH(DATA_WIDTH),
        .ADDR_WIDTH(MAP_ADDR_WIDTH)
    ) attr_ram (
        .clk  (clk),
        .we   (1'b0),
        .addr (attr_addr),
        .din  ({DATA_WIDTH{1'b0}}),
        .dout (attr_dout)
    );
endmodule
