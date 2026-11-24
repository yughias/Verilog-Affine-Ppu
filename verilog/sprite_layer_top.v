module sprite_layer_top #(
    parameter PPU_SIZE_Y = 256,
    parameter TOTAL_V_LINE = 525,
    parameter START_SCAN = 256,
    parameter UPDATE_Y = START_SCAN - 1,
    parameter START_SHIFT = 793
) (
    input wire clk,
    input wire rst,

    input wire [9:0] h_cnt,
    input wire [9:0] v_cnt,

    output wire [7:0] col_out,
    output wire lo_prio_out
);

    wire [4:0] oam_addr;
    wire [31:0] oam_dout;

    wire [10:0] tile_addr;
    wire [7:0] tile_dout;

    reg [8:0] y;

    wire not_vblank = v_cnt < PPU_SIZE_Y;
    wire last_vline = v_cnt == TOTAL_V_LINE - 1;
    wire active_line = not_vblank || last_vline;
    wire scan = h_cnt == PPU_SIZE_Y && active_line;
    wire shift = h_cnt == START_SHIFT && active_line; 
    wire sprite_layer_we = scan || shift;

    always @(posedge clk) begin
        if (rst) begin
            y <= 0;
        end else begin
            if (h_cnt == UPDATE_Y && active_line) begin
                y <= last_vline ? 0 : y + 1;
            end
        end
    end

    ram #(
        .DATA_WIDTH(32),
        .ADDR_WIDTH(5)
    ) oam_ram (
        .clk (clk),
        .we (1'b0),
        .addr (oam_addr),
        .din (0),
        .dout (oam_dout)
    );

    ram # (
        .DATA_WIDTH(8),
        .ADDR_WIDTH(11)
    ) tile_ram (
        .clk (clk),
        .we (1'b0),
        .addr (tile_addr),
        .din (0),
        .dout (tile_dout)
    );

    sprite_layer u_sprite_layer (
        .clk (clk),
        .rst (rst),
        .we (sprite_layer_we),
        .scan_in (scan),
        .shift_in (shift),
        .y_in (y),

        .oam_dout (oam_dout),
        .oam_addr (oam_addr),

        .tile_addr (tile_addr),
        .tile_dout (tile_dout),

        .col_out (col_out),
        .lo_prio_out (lo_prio_out)
    );

endmodule
