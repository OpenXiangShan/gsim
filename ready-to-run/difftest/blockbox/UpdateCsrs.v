module UpdateCsrs (
    input  [1:0]        priv,
    input  [75:0]       mstatus,
    input  [75:0]       mepc,
    input  [75:0]       mtval,
    input  [75:0]       mscratch,
    input  [75:0]       mcause,
    input  [75:0]       mtvec,
    input  [75:0]       mie,
    input  [75:0]       mip,
    input  [75:0]       medeleg,
    input  [75:0]       mideleg,
    input  [75:0]       sepc,
    input  [75:0]       stval,
    input  [75:0]       sscratch,
    input  [75:0]       stvec,
    input  [75:0]       satp,
    input  [75:0]       scause,
    input clock
);

endmodule