module InstFinish (
    input           clock,
    input           is_mmio,
    input           valid,
    input [63:0]    pc,
    input [31:0]    inst,
    input [11:0]    rcsr_id
);

import "DPI-C" function void update_indi(bit is_mmio, bit valid, int rcsr_id);
import "DPI-C" function void update_pc(longint pc, int inst);

always @(posedge clock) begin
    update_indi(is_mmio, valid, {20'b0, rcsr_id});
    update_pc(pc, inst);
end

endmodule