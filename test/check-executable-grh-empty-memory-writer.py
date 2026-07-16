#!/usr/bin/env python3

import json
import sys
from pathlib import Path


def require(condition, message):
    if not condition:
        raise SystemExit(
            f"executable GRH empty-memory-writer check failed: {message}"
        )


def main():
    require(
        len(sys.argv) == 2,
        "usage: check-executable-grh-empty-memory-writer.py <model.json>",
    )
    with Path(sys.argv[1]).open(encoding="utf-8") as stream:
        model = json.load(stream)

    require(model.get("format") == "gsim.executable-grh.v2", "unexpected format")
    require(model.get("boundary") == "PreCoarsen", "unexpected export boundary")
    require(model.get("analysisOnly") is False, "export is not executable")

    graphs = model.get("graphs")
    require(isinstance(graphs, list) and len(graphs) == 1, "expected one graph")
    graph = graphs[0]
    require(graph.get("symbol") == "EmptyMemoryWriter", "unexpected graph symbol")
    operations = graph.get("ops", [])

    memories = [op for op in operations if op.get("kind") == "kMemory"]
    reads = [op for op in operations if op.get("kind") == "kMemoryReadPort"]
    writes = [op for op in operations if op.get("kind") == "kMemoryWritePort"]
    require(len(memories) == 1, "backing memory was not preserved")
    require(len(reads) == 1, "live memory read was not preserved")
    require(not writes, "constant-false memory write was not eliminated")

    print("executable GRH empty-memory-writer PASS")


if __name__ == "__main__":
    main()
