#!/usr/bin/env python3

import json
import sys
from pathlib import Path


def require(condition, message):
    if not condition:
        raise SystemExit(f"executable GRH node-final assign elision check failed: {message}")


def main():
    require(len(sys.argv) == 2, "usage: check-executable-grh-node-final-assign-elision.py <model.json>")
    with Path(sys.argv[1]).open(encoding="utf-8") as stream:
        model = json.load(stream)

    graph = model["graphs"][0]
    values = graph["vals"]
    operations = graph["ops"]
    node_values = {
        value.get("attrs", {}).get("gsim.node_name", {}).get("v"): value["sym"]
        for value in values
        if "gsim.node_name" in value.get("attrs", {})
    }

    add_result = node_values["add_result"]
    add_producers = [operation for operation in operations if add_result in operation["out"]]
    require(len(add_producers) == 1, "add_result does not have exactly one producer")
    require(add_producers[0]["kind"] == "kAdd", "add_result producer was not retargeted kAdd")

    widened = node_values["widened"]
    widened_producers = [operation for operation in operations if widened in operation["out"]]
    require(len(widened_producers) == 1, "widened does not have exactly one producer")
    require(widened_producers[0]["kind"] == "kAssign", "width coercion kAssign was removed")
    require(widened_producers[0]["sym"].startswith("gsim.expr."), "coercion is not an expression op")

    constant = node_values["constant"]
    constant_assigns = [
        operation
        for operation in operations
        if operation["sym"].startswith("gsim.assign.") and constant in operation["out"]
    ]
    require(len(constant_assigns) == 1, "shared constant final assign was not retained")

    metadata = model["gsim"]
    require(metadata.get("nodeFinalAssignElidedCount", 0) >= 2, "elision counter was not updated")
    require(metadata.get("nodeFinalAssignKeptCount", 0) >= 1, "kept counter was not updated")
    require(
        metadata["operationCount"] == len(operations),
        "operationCount metadata does not match emitted operations",
    )
    require(
        metadata["valueCount"] == len(values),
        "valueCount metadata does not match emitted values",
    )

    print("executable GRH node-final assign elision PASS")


if __name__ == "__main__":
    main()
