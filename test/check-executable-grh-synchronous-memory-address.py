#!/usr/bin/env python3

import json
import sys
from pathlib import Path


def require(condition, message):
    if not condition:
        raise SystemExit(
            f"executable GRH synchronous-memory-address check failed: {message}"
        )


def attr_value(obj, name):
    attr = obj.get("attrs", {}).get(name)
    require(isinstance(attr, dict) and "v" in attr, f"malformed or missing {name}")
    return attr["v"]


def main():
    require(
        len(sys.argv) == 2,
        "usage: check-executable-grh-synchronous-memory-address.py <model.json>",
    )
    with Path(sys.argv[1]).open(encoding="utf-8") as stream:
        model = json.load(stream)

    graph = model["graphs"][0]
    require(graph.get("symbol") == "SynchronousMemoryAddress", "unexpected graph symbol")
    operations = graph.get("ops", [])

    address_registers = [
        operation
        for operation in operations
        if operation.get("kind") == "kRegister"
        and attr_value(operation, "gsim.node_name").endswith("$$ADDR")
    ]
    require(len(address_registers) == 1, "expected one generated memory address register")
    address_register = address_registers[0]

    address_reads = [
        operation
        for operation in operations
        if operation.get("kind") == "kRegisterReadPort"
        and attr_value(operation, "regSymbol") == address_register["sym"]
    ]
    address_writes = [
        operation
        for operation in operations
        if operation.get("kind") == "kRegisterWritePort"
        and attr_value(operation, "regSymbol") == address_register["sym"]
    ]
    memory_reads = [
        operation
        for operation in operations
        if operation.get("kind") == "kMemoryReadPort"
        and attr_value(operation, "gsim.role") == "prewrite_read_data"
    ]
    require(len(address_reads) == 1, "expected one address-register read")
    require(len(address_writes) == 1, "expected one address-register write")
    require(len(memory_reads) == 1, "expected one synchronous prewrite memory read")

    current_address = address_reads[0].get("out", [None])[0]
    write_inputs = address_writes[0].get("in", [])
    require(len(write_inputs) >= 2, "malformed address-register write")
    next_address = write_inputs[1]
    sampled_address = memory_reads[0].get("in", [None])[0]
    require(sampled_address == next_address, "memory read does not sample the current request")
    require(sampled_address != current_address, "memory read samples the previous request")

    print("executable GRH synchronous-memory-address PASS")


if __name__ == "__main__":
    main()
