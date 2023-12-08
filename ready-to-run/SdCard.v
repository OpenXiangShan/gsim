module SdCard (
    input [6:0]     addr,
    input           wen,
    input [63:0]    wdata,
    input           clock,
    input           cen,
    output[63:0]    rdata
);

endmodule