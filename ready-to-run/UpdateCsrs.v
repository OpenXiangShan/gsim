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
import "DPI-C" function void update_csr(int id, longint val);
import "DPI-C" function void update_priv(int val);
always @(posedge clock) begin
    update_csr({20'b0, mstatus[75:64]}, mstatus[63:0]);
    update_csr({20'b0, mepc[75:64]},    mepc[63:0]);
    update_csr({20'b0, mtval[75:64]},   mtval[63:0]);
    update_csr({20'b0, mscratch[75:64]}, mscratch[63:0]);
    update_csr({20'b0, mcause[75:64]},  mcause[63:0]);
    update_csr({20'b0, mtvec[75:64]},   mtvec[63:0]);
    update_csr({20'b0, mie[75:64]},     mie[63:0]);
    update_csr({20'b0, mip[75:64]},     mip[63:0]);
    update_csr({20'b0, medeleg[75:64]}, medeleg[63:0]);
    update_csr({20'b0, mideleg[75:64]}, mideleg[63:0]);
    update_csr({20'b0, sepc[75:64]},    sepc[63:0]);
    update_csr({20'b0, stval[75:64]},   stval[63:0]);
    update_csr({20'b0, sscratch[75:64]}, sscratch[63:0]);
    update_csr({20'b0, stvec[75:64]},   stvec[63:0]);
    update_csr({20'b0, satp[75:64]},    satp[63:0]);
    update_csr({20'b0, scause[75:64]},  scause[63:0]);
    update_priv({30'b0, priv});
end
endmodule