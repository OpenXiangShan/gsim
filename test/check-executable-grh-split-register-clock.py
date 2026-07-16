#!/usr/bin/env python3

import json
import sys
from pathlib import Path


def require(condition, message):
    if not condition:
        raise SystemExit(f"executable GRH split-register clock check failed: {message}")


def attr_value(obj, name):
    attr = obj.get("attrs", {}).get(name)
    require(isinstance(attr, dict), f"{obj.get('sym', '<unnamed>')} is missing {name}")
    if "v" in attr:
        return attr["v"]
    require("vs" in attr, f"{obj.get('sym', '<unnamed>')} has malformed {name}")
    return attr["vs"]


def main():
    require(len(sys.argv) == 2, "usage: check-executable-grh-split-register-clock.py <model.json>")
    path = Path(sys.argv[1])
    with path.open(encoding="utf-8") as stream:
        model = json.load(stream)

    require(model.get("format") == "gsim.executable-grh.v2", "unexpected format")
    require(model.get("stage") == "pre-coarsen", "export did not run at the pre-coarsen stage")
    require(model.get("boundary") == "PreCoarsen", "export did not run at the PreCoarsen boundary")
    require(model.get("analysisOnly") is False, "export is not executable")

    graphs = model.get("graphs")
    require(isinstance(graphs, list) and len(graphs) == 1, "expected one graph")
    graph = graphs[0]
    require(graph.get("symbol") == "SplitRegClock", "unexpected graph symbol")

    clock_ports = [
        port for port in graph.get("ports", {}).get("in", []) if port.get("name") == "clock"
    ]
    require(len(clock_ports) == 1, "expected one clock input port")
    clock_symbol = clock_ports[0].get("val")

    expected_names = {"value$127_0", "value$255_128"}
    register_sources = {}
    for value in graph.get("vals", []):
        attrs = value.get("attrs", {})
        node_type = attrs.get("gsim.node_type", {}).get("v")
        if node_type == "NODE_REG_SRC":
            register_sources[attrs.get("gsim.node_name", {}).get("v")] = value

    require(
        set(register_sources) == expected_names,
        f"expected split register sources {sorted(expected_names)}, got {sorted(register_sources)}",
    )
    require(
        all(value.get("w") == 128 for value in register_sources.values()),
        "split register widths are not 128 bits",
    )

    operations = graph.get("ops", [])
    for name in sorted(expected_names):
        declarations = [
            operation
            for operation in operations
            if operation.get("kind") == "kRegister"
            and operation.get("attrs", {}).get("gsim.node_name", {}).get("v") == name
        ]
        require(len(declarations) == 1, f"expected one kRegister declaration for {name}")
        register_symbol = declarations[0].get("sym")
        writes = [
            operation
            for operation in operations
            if operation.get("kind") == "kRegisterWritePort"
            and operation.get("attrs", {}).get("regSymbol", {}).get("v") == register_symbol
        ]
        require(len(writes) == 1, f"expected one register write port for {name}")
        write = writes[0]
        require(attr_value(write, "eventEdge") == ["posedge"], f"{name} is not posedge-triggered")
        inputs = write.get("in")
        require(isinstance(inputs, list) and len(inputs) == 4, f"{name} has an unexpected write ABI")
        require(inputs[-1] == clock_symbol, f"{name} did not inherit the original clock")

    print("executable GRH split-register clock inheritance PASS")


if __name__ == "__main__":
    main()
