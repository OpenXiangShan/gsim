#!/usr/bin/env python3

import json
import sys
from pathlib import Path


def require(condition, message):
    if not condition:
        raise SystemExit(f"executable GRH register-clock liveness check failed: {message}")


def attr_value(obj, name):
    attr = obj.get("attrs", {}).get(name)
    require(isinstance(attr, dict) and "v" in attr, f"malformed or missing {name}")
    return attr["v"]


def main():
    require(len(sys.argv) == 2, "usage: check-executable-grh-register-clock-liveness.py <model.json>")
    with Path(sys.argv[1]).open(encoding="utf-8") as stream:
        model = json.load(stream)

    graph = model["graphs"][0]
    require(graph.get("symbol") == "RegisterClock", "unexpected graph symbol")
    values = {
        attr_value(value, "gsim.node_name"): value
        for value in graph.get("vals", [])
        if "gsim.node_name" in value.get("attrs", {})
    }
    require("generated" in values, "generated clock register was removed as dead")
    require("child$state" in values, "child state register is missing")

    registers = {
        attr_value(operation, "gsim.node_name"): operation.get("sym")
        for operation in graph.get("ops", [])
        if operation.get("kind") == "kRegister"
    }
    child_writes = [
        operation
        for operation in graph.get("ops", [])
        if operation.get("kind") == "kRegisterWritePort"
        and attr_value(operation, "regSymbol") == registers.get("child$state")
    ]
    require(len(child_writes) == 1, "expected one child state write port")
    inputs = child_writes[0].get("in")
    require(isinstance(inputs, list) and len(inputs) == 4, "unexpected child write ABI")
    require(inputs[-1] == values["generated"]["sym"], "child write is not clocked by generated register")
    print("executable GRH register-clock liveness PASS")


if __name__ == "__main__":
    main()
