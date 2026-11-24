module bg_layer #(
    parameter MAP_W_TILES    = 64,
    parameter MAP_H_TILES    = 64,
    parameter TILE_SIZE      = 8,
    parameter DATA_WIDTH     = 8,
    parameter TILE_IDX_WIDTH = 8,
    parameter FRAC_BITS      = 16,

    parameter TILE_X_BITS     = $clog2(MAP_W_TILES),
    parameter TILE_Y_BITS     = $clog2(MAP_H_TILES),
    parameter MAP_ADDR_WIDTH  = TILE_X_BITS + TILE_Y_BITS,
    parameter TILE_ADDR_WIDTH = TILE_IDX_WIDTH + 2*$clog2(TILE_SIZE) - 1
) (
    input  wire                       clk,
    input  wire                       rst,

    input wire                        hblank,
    input wire                        vblank,

    output wire [MAP_ADDR_WIDTH-1:0]  map_addr,
    input  wire [TILE_IDX_WIDTH-1:0]  map_dout,

    output wire [TILE_ADDR_WIDTH-1:0] tile_addr,
    input  wire [DATA_WIDTH-1:0]      tile_dout,

    output wire [MAP_ADDR_WIDTH-1:0]  attr_addr,
    input  wire [TILE_IDX_WIDTH-1:0]  attr_dout,

    output wire [7:0] col_idx,

    input wire [31:0] in_scroll_x,
    input wire [31:0] in_scroll_y,
    input wire [31:0] in_m00,
    input wire [31:0] in_m01,
    input wire [31:0] in_m10,
    input wire [31:0] in_m11,
    input wire we_scroll
);

    localparam FINE_BITS   = $clog2(TILE_SIZE);

    reg prev_hblank;
    reg prev_vblank;
    reg [31:0] x;
    reg [31:0] y;
    reg [31:0] scroll_x;
    reg [31:0] scroll_y;
    reg [31:0] m00;
    reg [31:0] m01;
    reg [31:0] m10;
    reg [31:0] m11;
    reg [31:0] x_acc;
    reg [31:0] y_acc;

    always @(posedge clk) begin
        if (rst) begin
            scroll_x <= 0;
            scroll_y <= 0;
            m00      <= 32'h10000;
            m11      <= 32'h10000;
            m01      <= 0;
            m10      <= 0;
            x_acc    <= 0;
            y_acc    <= 0;
            x        <= 0;
            y        <= 0;
            prev_hblank <= 1'b0;
        end else begin
            if (we_scroll) begin
                scroll_x <= in_scroll_x;
                scroll_y <= in_scroll_y;
                m00      <= in_m00;
                m01      <= in_m01;
                m10      <= in_m10;
                m11      <= in_m11;
                x_acc    <= in_scroll_x;
                y_acc    <= in_scroll_y;
                x        <= in_scroll_x;
                y        <= in_scroll_y;
            end else if (vblank) begin
                x_acc       <= scroll_x;
                y_acc       <= scroll_y;
                x           <= scroll_x;
                y           <= scroll_y;
                prev_hblank <= 1'b0;
            end else begin
                if (!prev_hblank && hblank) begin
                    x_acc <= x_acc + m10;
                    y_acc <= y_acc + m11;
                    x     <= x_acc + m10;
                    y     <= y_acc + m11;
                end else if (!hblank) begin
                    x <= x + m00;
                    y <= y + m01;
                end
                prev_hblank <= hblank;
            end
        end
    end

    wire [31:0] scrolled_x = x;
    wire [31:0] scrolled_y = y;

    // Stage 0: Drive map/attr addresses & pipeline fine coordinates
    localparam TILE_START_BITS = FRAC_BITS + FINE_BITS;
    wire [TILE_X_BITS-1:0] tile_x = scrolled_x[TILE_START_BITS +: TILE_X_BITS];
    wire [TILE_Y_BITS-1:0] tile_y = scrolled_y[TILE_START_BITS +: TILE_Y_BITS];

    assign map_addr  = {tile_y, tile_x};
    assign attr_addr = {tile_y, tile_x};

    reg [FINE_BITS-1:0] fine_x_s1;
    reg [FINE_BITS-1:0] fine_y_s1;

    always @(posedge clk) begin
        if (rst) begin
            fine_x_s1 <= {FINE_BITS{1'b0}};
            fine_y_s1 <= {FINE_BITS{1'b0}};
        end else begin
            fine_x_s1 <= scrolled_x[18:16];
            fine_y_s1 <= scrolled_y[18:16];
        end
    end

    // Stage 1: map_dout & attr_dout are valid -> drive tile_addr
    wire       flip_x_s1 = attr_dout[0];
    wire       flip_y_s1 = attr_dout[1];
    wire [3:0] attr_s1 = attr_dout[7:4];

    wire [FINE_BITS-1:0] flipped_fine_y = {FINE_BITS{flip_y_s1}} ^ fine_y_s1;
    wire [FINE_BITS-1:0] flipped_fine_x = {FINE_BITS{flip_x_s1}} ^ fine_x_s1;
    assign tile_addr = {map_dout, flipped_fine_y, flipped_fine_x[FINE_BITS-1:1] };

    reg odd_x_s2;
    reg [3:0] attr_s2;

    always @(posedge clk) begin
        if (rst) begin
            odd_x_s2 <= 0;
            attr_s2 <= 0;
        end else begin
            odd_x_s2 <= flipped_fine_x[0];
            attr_s2 <= attr_s1;
        end
    end

    wire [3:0] tile_col_idx = odd_x_s2 ? tile_dout[7:4] : tile_dout[3:0];
    assign col_idx = { attr_s2, tile_col_idx };

endmodule
