module TransExcep(
    input           clock,
    input           intr,
    input [63:0]    cause,
    input [63:0]    pc
);

import "DPI-C" function void update_excep(bit intr, longint cause, longint pc);

always @(posedge clock) begin
    update_excep(intr, cause, pc);
end

endmodule