#!/usr/bin/env python3

import json
import sys
from pathlib import Path


def require(condition, message):
    if not condition:
        raise SystemExit(
            f"executable GRH async-reset constant-next check failed: {message}"
        )


def scalar_attr(obj, name):
    attr = obj.get("attrs", {}).get(name)
    require(isinstance(attr, dict), f"{obj.get('sym', '<unnamed>')} is missing {name}")
    require("v" in attr, f"{obj.get('sym', '<unnamed>')} has malformed {name}")
    return attr["v"]


def list_attr(obj, name):
    attr = obj.get("attrs", {}).get(name)
    require(isinstance(attr, dict), f"{obj.get('sym', '<unnamed>')} is missing {name}")
    require("vs" in attr, f"{obj.get('sym', '<unnamed>')} has malformed {name}")
    return attr["vs"]


def main():
    require(
        len(sys.argv) == 2,
        "usage: check-executable-grh-async-reset-constant-next.py <model.json>",
    )
    path = Path(sys.argv[1])
    with path.open(encoding="utf-8") as stream:
        model = json.load(stream)

    require(model.get("format") == "gsim.executable-grh.v2", "unexpected format")
    require(model.get("stage") == "pre-coarsen", "export did not run pre-coarsen")
    require(model.get("boundary") == "PreCoarsen", "unexpected export boundary")
    require(model.get("analysisOnly") is False, "export is not executable")

    graphs = model.get("graphs")
    require(isinstance(graphs, list) and len(graphs) == 1, "expected one graph")
    graph = graphs[0]
    require(graph.get("symbol") == "AsyncResetConstantNext", "unexpected graph symbol")

    input_ports = {
        port.get("name"): port.get("val")
        for port in graph.get("ports", {}).get("in", [])
    }
    require(input_ports.get("clock"), "missing clock input")
    require(input_ports.get("reset"), "missing async reset input")

    register_symbols = {}
    for operation in graph.get("ops", []):
        if operation.get("kind") != "kRegister":
            continue
        name = operation.get("attrs", {}).get("gsim.node_name", {}).get("v")
        if name in {"constantReg", "controlReg"}:
            register_symbols[name] = operation.get("sym")
    require(
        set(register_symbols) == {"constantReg", "controlReg"},
        f"unexpected register declarations: {sorted(register_symbols)}",
    )

    writes = {}
    for operation in graph.get("ops", []):
        if operation.get("kind") != "kRegisterWritePort":
            continue
        reg_symbol = operation.get("attrs", {}).get("regSymbol", {}).get("v")
        for name, expected_symbol in register_symbols.items():
            if reg_symbol == expected_symbol:
                require(name not in writes, f"duplicate write port for {name}")
                writes[name] = operation
    require(set(writes) == set(register_symbols), "missing register write port")

    for name, write in writes.items():
        inputs = write.get("in")
        require(isinstance(inputs, list) and len(inputs) == 5, f"{name} has unexpected ABI")
        require(inputs[-2] == input_ports["clock"], f"{name} is not clocked by clock")
        require(inputs[-1] == input_ports["reset"], f"{name} is not reset by reset")
        require(
            list_attr(write, "eventEdge") == ["posedge", "posedge"],
            f"{name} does not retain clock/reset posedges",
        )
        require(scalar_attr(write, "gsim.reset_kind") == "async", f"{name} is not async")

    require(
        scalar_attr(writes["constantReg"], "gsim.constant_normal_update") is True,
        "constantReg is not marked as a constant normal update",
    )
    require(
        "gsim.constant_normal_update" not in writes["controlReg"].get("attrs", {}),
        "controlReg is incorrectly marked as a constant normal update",
    )

    print("executable GRH async-reset constant-next PASS")


if __name__ == "__main__":
    main()
