module circuit;
input a,b,c;
output y;
wire dead;
assign dead = a & c;
assign y = ((a & 1'b0) | (b ^ b)) | c;
endmodule
