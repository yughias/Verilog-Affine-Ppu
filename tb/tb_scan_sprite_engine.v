`timescale 1ns/1ps
`default_nettype none

module tb_sprite_layer;

    // ------------------------------------------------------------------
    // Parameters of the test
    // ------------------------------------------------------------------
    localparam [8:0] SPR_X    = 9'd0;
    localparam [8:0] SPR_Y    = 9'd0;
    localparam [5:0] SPR_IDX  = 6'd8;
    localparam [3:0] SPR_PAL  = 4'd5;
    localparam integer SCAN_CYCLES = 2000;   // generous budget for a full OAM scan

    // ------------------------------------------------------------------
    // Clock / DUT signals
    // ------------------------------------------------------------------
    reg clk = 0;
    always #5 clk = ~clk;

    reg        rst;
    reg        we;
    reg        scan_in;
    reg        shift_in;
    reg  [8:0] y_in;

    wire [31:0] oam_dout;
    wire [4:0]  oam_addr;
    wire [7:0]  tile_dout;
    wire [10:0] tile_addr;
    wire [7:0]  col_out;

    sprite_layer #() dut (
        .clk       (clk),
        .rst       (rst),
        .we        (we),
        .scan_in   (scan_in),
        .shift_in  (shift_in),
        .y_in      (y_in),
        .oam_dout  (oam_dout),
        .oam_addr  (oam_addr),
        .tile_dout (tile_dout),
        .tile_addr (tile_addr),
        .col_out   (col_out)
    );

    wire [7:0] dbg_pbuf0 = dut.pixel_buffer[0];
    wire [7:0] dbg_pbuf1 = dut.pixel_buffer[1];
    wire [7:0] dbg_pbuf2 = dut.pixel_buffer[2];
    wire [7:0] dbg_pbuf3 = dut.pixel_buffer[3];

    wire [3:0] dbg_attr0 = dut.sprite_shifter_attr_in[0];
    wire [8:0] dbg_x0    = dut.sprite_shifter_x_in[0];

    // ------------------------------------------------------------------
    // Memories (ram module, synchronous read, 1 cycle latency)
    // ------------------------------------------------------------------
    ram #(.DATA_WIDTH(32), .ADDR_WIDTH(5)) u_oam (     // 32 entries
        .clk  (clk),
        .we   (1'b0),
        .addr (oam_addr),
        .din  (32'd0),
        .dout (oam_dout)
    );

    ram #(.DATA_WIDTH(8), .ADDR_WIDTH(11)) u_tile (    // 64 tiles x 32 bytes
        .clk  (clk),
        .we   (1'b0),
        .addr (tile_addr),
        .din  (8'd0),
        .dout (tile_dout)
    );

    // ------------------------------------------------------------------
    // "0" glyph, 8x8, one nibble per pixel, leftmost pixel = MSB nibble.
    // Asymmetric colors (not only 0/F) so bit-order bugs can't hide.
    // ------------------------------------------------------------------
    reg [31:0] glyph [0:7];
    initial begin
        glyph[0] = 32'h00123400;
        glyph[1] = 32'h05000060;
        glyph[2] = 32'h07000890;
        glyph[3] = 32'h0A00B0C0;
        glyph[4] = 32'h0D0E00F0;
        glyph[5] = 32'h01200030;
        glyph[6] = 32'h04000050;
        glyph[7] = 32'h00687900;
    end

    function [3:0] pix;
        input integer r;
        input integer c;
        begin
            pix = glyph[r][31 - 4*c -: 4];
        end
    endfunction

    // expected bitplane n for row r (MSB = leftmost pixel)
    function [7:0] plane;
        input integer r;
        input integer n;
        integer c;
        begin
            plane = 8'h00;
            for (c = 0; c < 8; c = c + 1)
                plane[7 - c] = pix(r, c) >> n;
        end
    endfunction

    // ------------------------------------------------------------------
    // Memory init (backdoor into the ram instances)
    //   Tile format: 32 bytes, 4 bytes per row, 4 bpp,
    //   low nibble = left pixel of the pair
    //   OAM format: {big, lo_prio, fy, fx, pal[3:0], idx[5:0], y[8:0], x[8:0]}
    // ------------------------------------------------------------------
    integer a, r, b;
    initial begin
        #1;  // let glyph be initialized first

        for (a = 0; a < 2048; a = a + 1) u_tile.memory[a] = 8'h00;

        for (r = 0; r < 8; r = r + 1)
            for (b = 0; b < 4; b = b + 1)
                u_tile.memory[SPR_IDX*32 + r*4 + b] = { pix(r, 2*b + 1), pix(r, 2*b) };

        // all sprites off-screen (y = 256)
        for (a = 0; a < 32; a = a + 1)
            u_oam.memory[a] = {1'b0, 1'b0, 1'b0, 1'b0, 4'd0, 6'd0, 9'd256, 9'd0};

        // the single visible sprite
        u_oam.memory[0] = {1'b0, 1'b0, 1'b0, 1'b0, SPR_PAL, SPR_IDX, SPR_Y, SPR_X};
    end

    // ------------------------------------------------------------------
    // Monitor: count load pulses per shifter
    // ------------------------------------------------------------------
    integer we_count [0:7];
    integer k;

    always @(posedge clk) begin
        for (k = 0; k < 8; k = k + 1)
            if (dut.sprite_shifter_we[k])
                we_count[k] = we_count[k] + 1;
    end

    // ------------------------------------------------------------------
    // Checks
    // ------------------------------------------------------------------
    integer errors = 0;

    task check;
        input            cond;
        input [8*64-1:0] msg;
        begin
            if (cond !== 1'b1) begin
                errors = errors + 1;
                $display("[%0t] ERROR: %0s", $time, msg);
            end
        end
    endtask

    task scan_line;
        input [8:0] line;
        integer row, c;
        reg [3:0] got;
        begin
            row = line - SPR_Y;

            // reset
            @(negedge clk);
            rst = 1; we = 0; scan_in = 0; shift_in = 0; y_in = 0;
            repeat (4) @(negedge clk);
            rst = 0;
            for (k = 0; k < 8; k = k + 1) we_count[k] = 0;

            // start scan of the line (shift_in = 0 -> shift register cleared)
            @(negedge clk);
            we = 1; scan_in = 1; shift_in = 0; y_in = line;
            @(negedge clk);
            we = 0; scan_in = 0;

            repeat (SCAN_CYCLES) @(posedge clk);

            $display("--- line %0d ---", line);

            if (row >= 0 && row < 8) begin
                check(we_count[0] == 1, "shifter 0 not loaded exactly once");
                for (k = 1; k < 8; k = k + 1)
                    check(we_count[k] == 0, "unexpected load on shifter 1..7");

                check(dut.sprite_shifter_arr[0].u_sprite_shifter.attr == SPR_PAL,
                      "shifter 0 palette mismatch");
                check(dut.sprite_shifter_arr[0].u_sprite_shifter.x_counter == SPR_X,
                      "shifter 0 x mismatch");

                check(dut.sprite_shifter_arr[0].u_sprite_shifter.p0 == plane(row, 0), "plane 0 mismatch");
                check(dut.sprite_shifter_arr[0].u_sprite_shifter.p1 == plane(row, 1), "plane 1 mismatch");
                check(dut.sprite_shifter_arr[0].u_sprite_shifter.p2 == plane(row, 2), "plane 2 mismatch");
                check(dut.sprite_shifter_arr[0].u_sprite_shifter.p3 == plane(row, 3), "plane 3 mismatch");

                $display("  planes: got %h %h %h %h  exp %h %h %h %h",
                         dut.sprite_shifter_arr[0].u_sprite_shifter.p0,
                         dut.sprite_shifter_arr[0].u_sprite_shifter.p1,
                         dut.sprite_shifter_arr[0].u_sprite_shifter.p2,
                         dut.sprite_shifter_arr[0].u_sprite_shifter.p3,
                         plane(row, 0), plane(row, 1), plane(row, 2), plane(row, 3));

                // enable shifting: 'shift' is a register in sprite_layer,
                // written only when we = 1
                @(negedge clk);
                we = 1; scan_in = 0; shift_in = 1; y_in = line;
                @(negedge clk);           // posedge in between: shift <= 1
                we = 0;

                // sanity: the shift register is set and actually reaches the shifter
                check(dut.shift === 1'b1, "sprite_layer.shift not set by we/shift_in");
                check(dut.sprite_shifter_arr[0].u_sprite_shifter.en === 1'b1,
                      "shifter 0 en not driven by sprite_layer.shift");

                // pixel 0 is already on col_out; every posedge shifts by one
                for (c = 0; c < 8; c = c + 1) begin
                    got = col_out[3:0];
                    if (got !== pix(row, c)) begin
                        errors = errors + 1;
                        $display("[%0t] ERROR: line %0d pixel %0d got %h exp %h",
                                 $time, line, c, got, pix(row, c));
                    end
                    check(col_out[7:4] == SPR_PAL || col_out[3:0] == 4'h0,
                          "palette bits on col_out mismatch");
                    @(negedge clk);
                end

                // after 8 shifts the sprite must be fully out (transparent)
                check(col_out[3:0] == 4'h0, "pixels left after 8 shifts");

                // disable shifting
                we = 1; shift_in = 0;
                @(negedge clk);
                we = 0;
            end else begin
                for (k = 0; k < 8; k = k + 1)
                    check(we_count[k] == 0, "shifter loaded on a line without sprites");
            end
        end
    endtask

    // ------------------------------------------------------------------
    // Main
    // ------------------------------------------------------------------
    integer line;
    initial begin
        $dumpfile("tb_sprite_layer.vcd");
        $dumpvars(0, tb_sprite_layer);

        rst = 1; we = 0; scan_in = 0; shift_in = 0; y_in = 0;

        // every row of the sprite + one line just below it
        for (line = 0; line <= 8; line = line + 1)
            scan_line(line);

        if (errors == 0) $display("PASS");
        else             $display("FAIL: %0d errors", errors);
        $finish;
    end

endmodule

`default_nettype wire