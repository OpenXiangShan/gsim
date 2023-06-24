module SdCard (
    input [6:0]     addr,
    input           wen,
    input [63:0]    wdata,
    input           clock,
    input           cen,
    output[63:0]    rdata
);

import "DPI-C" function void sdcard_read(input int offset, output longint rdata);
import "DPI-C" function void sdcard_write(int offset, longint wdata);
always @(posedge clock) begin
    if(cen) begin
        sdcard_read({25'b0, addr}, rdata);
    end
    if(wen) begin
        sdcard_write({25'b0, addr}, wdata);
    end
end

endmodule