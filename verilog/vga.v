module vga (
    input  wire                  clk_25mhz,
    input  wire                  rst,

    // VGA Physical Pins
    output wire                  hsync,
    output wire                  vsync, 

    output reg [9:0]            h_cnt,
    output reg [9:0]            v_cnt
);

    // Standard 640x480 @ 60Hz Timings
    // H_BACK = 48
    // V_BACK = 33
    localparam H_VISIBLE = 640, H_FRONT = 16, H_SYNC = 96, H_TOTAL = 800;
    localparam V_VISIBLE = 480, V_FRONT = 10, V_SYNC = 2,  V_TOTAL = 525;

    always @(posedge clk_25mhz) begin
        if (rst) begin
            h_cnt <= 10'd0;
            v_cnt <= 10'd0;
        end else begin
            if (h_cnt == H_TOTAL - 1) begin
                h_cnt <= 10'd0;
                v_cnt <= (v_cnt == V_TOTAL - 1) ? 10'd0 : (v_cnt + 10'd1);
            end else begin
                h_cnt <= h_cnt + 10'd1;
            end
        end
    end

    assign hsync = ~((h_cnt >= H_VISIBLE + H_FRONT) && (h_cnt < H_VISIBLE + H_FRONT + H_SYNC));
    assign vsync = ~((v_cnt >= V_VISIBLE + V_FRONT) && (v_cnt < V_VISIBLE + V_FRONT + V_SYNC));

endmodule
