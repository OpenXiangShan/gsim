#!/usr/bin/env python3

import json
import sys
from pathlib import Path


def require(condition, message):
    if not condition:
        raise SystemExit(f"executable GRH effects check failed: {message}")


def attr(obj, name):
    value = obj.get("attrs", {}).get(name)
    require(isinstance(value, dict), f"{obj.get('sym', '<unnamed>')} is missing {name}")
    return value


def attr_value(obj, name):
    value = attr(obj, name)
    if "v" in value:
        return value["v"]
    require("vs" in value, f"{obj.get('sym', '<unnamed>')} has malformed {name}")
    return value["vs"]


def constants_by_output(operations):
    constants = {}
    for operation in operations:
        if operation.get("kind") != "kConstant":
            continue
        outputs = operation.get("out")
        require(
            isinstance(outputs, list) and len(outputs) == 1,
            f"{operation.get('sym', '<unnamed>')} has an unexpected constant ABI",
        )
        constants[outputs[0]] = attr_value(operation, "constValue")
    return constants


def task_by_node_name(tasks, name):
    matches = [task for task in tasks if attr_value(task, "gsim.node_name") == name]
    require(len(matches) == 1, f"expected one system task for {name}, got {len(matches)}")
    return matches[0]


def task_constant_inputs(task, constants):
    return [constants[symbol] for symbol in task.get("in", []) if symbol in constants]


def main():
    require(len(sys.argv) == 2, "usage: check-executable-grh-effects.py <model.json>")
    path = Path(sys.argv[1])
    with path.open(encoding="utf-8") as stream:
        model = json.load(stream)

    require(model.get("format") == "gsim.executable-grh.v2", "unexpected format")
    require(model.get("stage") == "pre-coarsen", "export did not run at pre-coarsen")
    require(model.get("boundary") == "PreCoarsen", "unexpected export boundary")
    require(model.get("analysisOnly") is False, "export is not executable")

    graphs = model.get("graphs")
    require(isinstance(graphs, list) and len(graphs) == 1, "expected one graph")
    graph = graphs[0]
    require(graph.get("symbol") == "ExecutableGrhEffects", "unexpected graph symbol")

    values = graph.get("vals")
    operations = graph.get("ops")
    require(isinstance(values, list), "graph vals is not a list")
    require(isinstance(operations, list), "graph ops is not a list")

    input_ports = graph.get("ports", {}).get("in", [])
    wide_ports = [port for port in input_ports if port.get("name") == "wide"]
    clock_ports = [port for port in input_ports if port.get("name") == "clock"]
    require(len(wide_ports) == 1, "expected one wide input port")
    require(len(clock_ports) == 1, "expected one clock input port")
    wide_symbol = wide_ports[0].get("val")
    clock_symbol = clock_ports[0].get("val")

    wide_values = [value for value in values if value.get("sym") == wide_symbol]
    require(len(wide_values) == 1, "wide port does not reference one value")
    require(
        wide_values[0].get("w") == 128 and wide_values[0].get("in") is True,
        "wide input did not retain its 128-bit width",
    )

    tasks = [operation for operation in operations if operation.get("kind") == "kSystemTask"]
    task_names = [attr_value(task, "gsim.node_name") for task in tasks]
    require(
        sorted(task_names) == ["formatted_assert", "wide_print"],
        f"expected only active tasks, got {sorted(task_names)}",
    )

    constants = constants_by_output(operations)
    require("dead=%d\n" not in constants.values(), "optimizer-elided printf format was emitted")

    wide_task = task_by_node_name(tasks, "wide_print")
    require(attr_value(wide_task, "name") == "fwrite", "wide printf is not an fwrite")
    require(attr_value(wide_task, "gsim.effect_kind") == "printf", "wide task kind mismatch")
    require(attr_value(wide_task, "eventEdge") == ["posedge"], "wide printf is not posedge-triggered")
    wide_inputs = wide_task.get("in")
    require(isinstance(wide_inputs, list), "wide printf inputs is not a list")
    require(wide_inputs.count(wide_symbol) == 1, "wide printf does not directly consume the wide port")
    require(wide_inputs[-1] == clock_symbol, "wide printf did not retain its event clock")
    require(
        "wide=%d\n" in task_constant_inputs(wide_task, constants),
        "wide printf format is missing",
    )

    assert_task = task_by_node_name(tasks, "formatted_assert")
    require(attr_value(assert_task, "name") == "fatal", "assert did not lower to fatal")
    require(attr_value(assert_task, "gsim.effect_kind") == "assert", "assert task kind mismatch")
    literalized = attr(assert_task, "gsim.unbound_format_conversions_literalized")
    require(
        literalized.get("t") == "bool" and literalized.get("v") is True,
        "assert literalization marker is not boolean true",
    )
    require(
        "fatal d=%%d x=%%x\n" in task_constant_inputs(assert_task, constants),
        "assert's unbound %d/%x conversions were not literalized",
    )

    print("executable GRH optimizer-elided, wide printf, and assert effects PASS")


if __name__ == "__main__":
    main()
