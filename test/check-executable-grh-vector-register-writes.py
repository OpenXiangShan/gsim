#!/usr/bin/env python3

import json
import sys


def require(condition, message):
    if not condition:
        raise SystemExit(f"executable GRH vector-register-writes check failed: {message}")


def attr_value(obj, name):
    attr = obj.get("attrs", {}).get(name)
    require(isinstance(attr, dict) and "v" in attr, f"malformed or missing {name}")
    return attr["v"]


def main():
    require(len(sys.argv) == 2, "usage: check-executable-grh-vector-register-writes.py <model.json>")
    with open(sys.argv[1]) as handle:
        model = json.load(handle)
    graph = model["graphs"][0]
    vals = {v["sym"]: v for v in graph["vals"]}
    ops = graph["ops"]
    producing = {op["out"][0]: op for op in ops if op.get("out")}

    def const_literal(sym):
        op = producing.get(sym)
        if not op or op["kind"] != "kConstant":
            return None
        return attr_value(op, "constValue")

    def node_name(sym):
        return vals.get(sym, {}).get("attrs", {}).get("gsim.node_name", {}).get("v")

    # node id -> symbol maps via the register declarations
    writes = [op for op in ops if op["kind"] == "kRegisterWritePort"]
    by_reg = {}
    for op in writes:
        by_reg.setdefault(attr_value(op, "regSymbol"), []).append(op)

    # locate the vector registers by their REG_SRC node names
    v_src = next(v for v in graph["vals"] if node_name(v["sym"]) == "v")
    r_src = next(v for v in graph["vals"] if node_name(v["sym"]) == "r")
    s_src = next(v for v in graph["vals"] if node_name(v["sym"]) == "s")
    v_reg = "gsim.reg." + str(attr_value(v_src, "gsim.node_id"))
    r_reg = "gsim.reg." + str(attr_value(r_src, "gsim.node_id"))
    s_reg = "gsim.reg." + str(attr_value(s_src, "gsim.node_id"))

    # --- vector register v: one masked write per when leaf, source order ----
    v_writes = by_reg.get(v_reg, [])
    require(len(v_writes) == 4, f"v must have 4 write ports (one per when leaf), got {len(v_writes)}")
    masks = [const_literal(op["in"][2]) for op in v_writes]
    require(masks[0] == "32'hff00", f"v leaf0 mask must be 32'hff00, got {masks[0]}")
    require(masks[1] == "32'hff00", f"v leaf1 mask must be 32'hff00, got {masks[1]}")
    require(masks[2] == "32'hff0000", f"v leaf2 mask must be 32'hff0000, got {masks[2]}")
    dyn_mask = producing.get(v_writes[3]["in"][2])
    require(dyn_mask and dyn_mask["kind"] == "kShl",
            "v leaf3 mask must be a dynamic shift for the indexed write")
    for op in v_writes:
        require(attr_value(op, "gsim.reset_kind") == "none", "v leaves are not reset writes")
        require(op["attrs"]["eventEdge"]["vs"] == ["posedge"], "v leaves ride the clock posedge")

    # --- vector register r: one normal leaf + final all-ones sync reset -----
    r_writes = by_reg.get(r_reg, [])
    require(len(r_writes) == 2, f"r must have 2 write ports (leaf + sync reset), got {len(r_writes)}")
    require(attr_value(r_writes[0], "gsim.reset_kind") == "none", "r leaf0 is the normal write")
    require(attr_value(r_writes[1], "gsim.reset_kind") == "sync", "r leaf1 must be the sync reset")
    require(const_literal(r_writes[1]["in"][2]) == "32'hffffffff",
            "sync reset write must use an all-ones mask (wins over normal leaves)")

    # --- vector register ra: async reset = last leaf with two posedge events --
    ra_src = next(v for v in graph["vals"] if node_name(v["sym"]) == "ra")
    ra_reg = "gsim.reg." + str(attr_value(ra_src, "gsim.node_id"))
    ra_writes = by_reg.get(ra_reg, [])
    require(len(ra_writes) == 2, f"ra must have 2 write ports (leaf + async reset), got {len(ra_writes)}")
    require(attr_value(ra_writes[0], "gsim.reset_kind") == "none", "ra leaf0 is the normal write")
    require(ra_writes[0]["attrs"]["eventEdge"]["vs"] == ["posedge"],
            "ra normal leaf rides only the clock posedge")
    require(attr_value(ra_writes[1], "gsim.reset_kind") == "async", "ra leaf1 must be the async reset")
    require(ra_writes[1]["attrs"]["eventEdge"]["vs"] == ["posedge", "posedge"],
            "async reset leaf must ride clock posedge + reset posedge")
    require(const_literal(ra_writes[1]["in"][2]) == "32'hffffffff",
            "async reset write must use an all-ones mask (wins over normal leaves)")
    require(len(ra_writes[1]["in"]) == 5,
            "async reset leaf must carry the reset signal as a second event operand")

    # --- scalar register s: untouched single-write path ---------------------
    s_writes = by_reg.get(s_reg, [])
    require(len(s_writes) == 1, f"scalar s must keep a single write port, got {len(s_writes)}")
    require(const_literal(s_writes[0]["in"][2]) == "8'hff",
            "scalar write must keep the all-ones mask")

    # --- no functional-merge mux chain feeds a static vector write's data ---
    v_static_datas = [producing[v_writes[i]["in"][1]]["kind"] for i in range(3)]
    require(all(kind == "kConcat" for kind in v_static_datas),
            f"static leaves must pack data by concat, got {v_static_datas}")

    print("executable GRH vector-register mask-write structure OK")


if __name__ == "__main__":
    main()
