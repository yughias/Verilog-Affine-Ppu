module color_mixer #(
    parameter ATTR_BITS = 4,
    parameter COL_BITS = 4
) (
    input wire clk,
    input wire rst,

    input wire [7:0] fg_idx,
    input wire [7:0] bg_idx,
    input wire [7:0] sprite_idx,
    input wire sprite_lo_prio_in,

    output reg [2:0] r,
    output reg [2:0] g,
    output reg [1:0] b
);
    localparam COL_RAM_ADDR_SPACE = ATTR_BITS + COL_BITS;

    wire [7:0] fg_rgb332;
    wire [7:0] bg_rgb332;
    wire [7:0] sprite_rgb332;

    ram #(
        .ADDR_WIDTH(COL_RAM_ADDR_SPACE)
    ) fg_col_ram (
        .clk  (clk),
        .we   (1'b0),
        .addr (fg_idx),
        .din  (0),
        .dout (fg_rgb332)
    );

    ram #(
        .ADDR_WIDTH(COL_RAM_ADDR_SPACE)
    ) bg_col_ram (
        .clk  (clk),
        .we   (1'b0),
        .addr (bg_idx),
        .din  (0),
        .dout (bg_rgb332)
    );

    ram #(
        .ADDR_WIDTH(COL_RAM_ADDR_SPACE)
    ) sprite_col_ram (
        .clk  (clk),
        .we   (1'b0),
        .addr (sprite_idx),
        .din  (0),
        .dout (sprite_rgb332)
    );

    // stage 0: forward transparency and read
    reg fg_transparent;
    reg sprite_transparent;
    reg sprite_lo_prio;
    always @(posedge clk) begin
        if (rst) begin
            fg_transparent <= 0;
            sprite_transparent <= 0;
            sprite_lo_prio <= 0;
        end else begin
            fg_transparent <= fg_idx[3:0] == {COL_BITS{1'b0}};
            sprite_transparent <= sprite_idx[3:0] == {COL_BITS{1'b0}};
            sprite_lo_prio <= sprite_lo_prio_in;
        end
    end

    // stage 1: read and compute final color stage
    wire [7:0] final_col = sprite_transparent ? 
                (fg_transparent ? bg_rgb332 : fg_rgb332) // sprite is transparent render fg or bg
                :
                (sprite_lo_prio && !fg_transparent ? fg_rgb332 :sprite_rgb332); // if sprite is low priority and fg not transparent render fg otherwise sprite

    always @(posedge clk) begin
        if (rst) begin
            r <= 0;
            g <= 0;
            b <= 0;
        end else begin
            r <= final_col[7:5];
            g <= final_col[4:2];
            b <= final_col[1:0];
        end
    end

endmodule
