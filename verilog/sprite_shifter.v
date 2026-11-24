module sprite_shifter (
    input wire clk,
    input wire rst,

    input wire we,
    input wire en,
    input wire [7:0] p0_in,
    input wire [7:0] p1_in,
    input wire [7:0] p2_in,
    input wire [7:0] p3_in,
    input wire [3:0] attr_in,
    input wire lo_prio_in,
    input wire [8:0] x_in,

    output wire [7:0] pixel_out,
    output wire lo_prio_out
);
    reg [3:0] attr;
    reg lo_prio;
    reg [8:0] x_counter;
    
    reg [7:0] p0;
    reg [7:0] p1;
    reg [7:0] p2;
    reg [7:0] p3;

    assign pixel_out = x_counter != 0 ? 0 : { attr, p3[7], p2[7], p1[7], p0[7] };
    assign lo_prio_out = lo_prio;

    always @(posedge clk) begin
        if (rst) begin
            p0 <= 0;
            p1 <= 0;
            p2 <= 0;
            p3 <= 0;
            x_counter <= 0;
            attr <= 0;
        end else begin
            if (we) begin
                x_counter <= x_in;
                attr <= attr_in;
                lo_prio <= lo_prio_in;
                p0 <= p0_in;
                p1 <= p1_in;
                p2 <= p2_in;
                p3 <= p3_in;
            end else if (en) begin
                if (x_counter != 0) begin
                    x_counter <= x_counter - 1;
                end else begin
                    p0 <= {p0[6:0], 1'b0};
                    p1 <= {p1[6:0], 1'b0};
                    p2 <= {p2[6:0], 1'b0};
                    p3 <= {p3[6:0], 1'b0};
                end
            end
        end
    end

endmodule
