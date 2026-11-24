module sprite_layer (
    input wire clk,
    input wire rst,

    input wire we,
    input wire scan_in,
    input wire shift_in,
    input wire [8:0] y_in,

    input wire [31:0] oam_dout,
    output wire [4:0] oam_addr,

    input wire [7:0] tile_dout,
    output wire [10:0] tile_addr,

    output wire [7:0] col_out, // palette + col_idx
    output wire lo_prio_out
);
    reg [7:0] sprite_shifter_en;
    reg [7:0] sprite_shifter_we;
    reg [3:0] sprite_shifter_attr_in [7:0];
    reg [7:0] sprite_shifter_lo_prio_in;
    reg [8:0] sprite_shifter_x_in [7:0];

    wire [7:0] sprite_shifter_col_idx [7:0];
    wire [7:0] sprite_shifter_lo_prio;

    reg [4:0] oam_addr_reg;
    assign oam_addr = oam_addr_reg;

    // buffer to temporarily store tile pixels row
    reg [7:0] pixel_buffer [3:0];

    genvar i;
    integer k;

    reg scan;
    reg shift;

    generate
        for (i = 0; i < 8; i = i + 1) begin : sprite_shifter_arr
            sprite_shifter u_sprite_shifter (
                .clk (clk),
                .rst (rst),
                .en (shift),
                .we (sprite_shifter_we[i]),
                .attr_in (sprite_shifter_attr_in[i]),
                .lo_prio_in (sprite_shifter_lo_prio_in[i]),
                .x_in (sprite_shifter_x_in[i]),

                .p0_in (pixel_buffer[0]),
                .p1_in (pixel_buffer[1]),
                .p2_in (pixel_buffer[2]),
                .p3_in (pixel_buffer[3]),

                .pixel_out (sprite_shifter_col_idx[i]),
                .lo_prio_out (sprite_shifter_lo_prio[i])
            );
        end
    endgenerate

    reg [8:0] y;
    reg [4:0] oam_counter;
    reg [2:0] sprite_counter;
    wire scan_finish = oam_counter == 31 || sprite_counter == 7;

    localparam IDLE  = 0,
               WAIT_OAM = 1,
               OAM_CHECK = 2,
               WAIT_TILE = 3,
               TILE_LOAD = 4,
               FILL_SHIFTER = 5;
    reg [7:0] state;

    // tile scan reg helpers
    reg [10:0] tile_addr_reg;
    assign tile_addr = tile_addr_reg;
    reg [1:0] tile_idx;

    // oam decode
    wire [8:0] sprite_x = oam_dout[8:0];
    wire [8:0] sprite_y = oam_dout[17:9];
    wire [5:0] sprite_idx = oam_dout[23:18];
    wire [3:0] sprite_pal = oam_dout[27:24];
    wire sprite_fx = oam_dout[28];
    wire sprite_fy = oam_dout[29];
    wire sprite_lo_prio = oam_dout[30];
    wire sprite_big = oam_dout[31];
    wire [8:0] sprite_row = y - sprite_y;
    wire sprite_valid = sprite_row < 8;

    // tile access wires
    wire [2:0] even_load_addr = 7 - tile_idx*2;
    wire [2:0] odd_load_addr = 6 - tile_idx*2;

    wire [2:0] flip_x_mask = {3{sprite_fx}};
    wire [2:0] flip_y_mask = {3{sprite_fy}};

    always @(posedge clk) begin
        if (rst) begin
            y <= 0;
            oam_counter <= 0;
            sprite_counter <= 0;
            scan <= 0;
            shift <= 0;
            state <= 0;
            for (k = 0; k < 4; k = k + 1)
                pixel_buffer[k] <= 0;
            tile_idx <= 0;
        end else begin
            // default
            sprite_shifter_we <= 0;

            if (we) begin
                scan <= scan_in;
                y <= y_in;
                shift <= shift_in;
            end

            case (state)
                IDLE: begin
                    oam_counter <= 0;
                    sprite_counter <= 0;
                    oam_addr_reg <= 0;
                    if (scan) begin
                        scan <= 0;
                        state <= WAIT_OAM;
                    end
                end

                WAIT_OAM: begin
                    state <= OAM_CHECK;
                end

                OAM_CHECK: begin
                    if (sprite_valid) begin
                        tile_addr_reg <= { sprite_idx, sprite_row[2:0] ^ flip_y_mask, 2'b0 };
                        tile_idx <= 0;
                        state <= WAIT_TILE;
                    end else begin
                        oam_addr_reg <= oam_addr_reg + 1;
                        oam_counter <= oam_counter + 1;
                        state <= oam_counter == 31 ? IDLE : WAIT_OAM;
                    end
                end

                WAIT_TILE: begin
                    state <= TILE_LOAD;
                end

                TILE_LOAD: begin
                    pixel_buffer[0][even_load_addr ^ flip_x_mask] <= tile_dout[0];
                    pixel_buffer[1][even_load_addr ^ flip_x_mask] <= tile_dout[1];
                    pixel_buffer[2][even_load_addr ^ flip_x_mask] <= tile_dout[2];
                    pixel_buffer[3][even_load_addr ^ flip_x_mask] <= tile_dout[3];
                    pixel_buffer[0][odd_load_addr ^ flip_x_mask] <= tile_dout[4];
                    pixel_buffer[1][odd_load_addr ^ flip_x_mask] <= tile_dout[5];
                    pixel_buffer[2][odd_load_addr ^ flip_x_mask] <= tile_dout[6];
                    pixel_buffer[3][odd_load_addr ^ flip_x_mask] <= tile_dout[7];

                    if (tile_idx != 3) begin
                        tile_idx <= tile_idx + 1;
                        tile_addr_reg <= tile_addr_reg + 1;
                        state <= WAIT_TILE;
                    end else begin
                        tile_idx <= 0;
                        state <= FILL_SHIFTER;
                    end
                end

                FILL_SHIFTER: begin
                    sprite_shifter_we[sprite_counter] <= 1;
                    sprite_shifter_attr_in[sprite_counter] <= sprite_pal;
                    sprite_shifter_x_in[sprite_counter] <= sprite_x;
                    sprite_shifter_lo_prio_in[sprite_counter] <= sprite_lo_prio;

                    oam_counter <= oam_counter + 1;
                    sprite_counter <= sprite_counter + 1;
                    oam_addr_reg <= oam_addr_reg + 1;
                    state <= scan_finish ? IDLE : WAIT_OAM;
                end

                default: begin
                    state <= IDLE;
                    sprite_shifter_we[7:0] <= 0;
                    sprite_shifter_en[7:0] <= 0;
                end

            endcase
        end
    end

    reg [7:0] col_out_r;
    reg lo_prio_out_r;
    integer m;

    always @(*) begin
        col_out_r = 0;
        lo_prio_out_r = 0;
        for (m = 7; m >= 0; m = m - 1)
            if (sprite_shifter_col_idx[m][3:0] != 0) begin
                col_out_r = sprite_shifter_col_idx[m];
                lo_prio_out_r = sprite_shifter_lo_prio[m];
            end
    end

    assign col_out = col_out_r;
    assign lo_prio_out = lo_prio_out_r;

endmodule
