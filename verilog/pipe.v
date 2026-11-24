module pipe #(
    parameter N = 8
) (
    input  wire clk,
    input  wire rst,
    input  wire din,
    input  wire en,
    output wire dout
);
    reg [N-1:0] shifter;

    assign dout = shifter [N-1];

    always @(posedge clk) begin
        if (rst) begin
            shifter <= 0;
        end else if (en) begin
            shifter <= { shifter[N-2:0], din };
        end
    end

endmodule
