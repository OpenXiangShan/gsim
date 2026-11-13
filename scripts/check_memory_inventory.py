"""Verify physical memories and external member IDs in a GSIM JSON dump."""
from __future__ import annotations

import argparse
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    args = parser.parse_args()
    graph = json.loads(args.input.read_text())
    assert graph["memoryInventoryComplete"] is True
    assert graph["sourceScopeInventoryComplete"] is True
    scopes = {scope["path"]: scope for scope in graph["sourceScopes"]}
    assert set(scopes) == {"", "ext"}
    assert scopes["ext"]["module"] == "Echo" and scopes["ext"]["kind"] == "external"
    assert scopes["ext"]["line"] > 0 and scopes["ext"]["instanceLine"] > 0
    nodes = {node["id"]: node for node in graph["nodes"]}
    memories = {node["name"]: node for node in graph["memories"]}
    assert set(memories) == {"mem", "delayed"}, memories
    for name, latency in (("mem", 0), ("delayed", 1)):
        memory = memories[name]
        assert memory["type"] == "NODE_MEMORY"
        assert memory["status"] == "VALID_NODE"
        assert memory["width"] == 8 and memory["dimension"] == []
        assert memory["depth"] == 4 and memory["readLatency"] == latency
        assert memory["writeLatency"] == 1 and memory["readUnderWrite"] == "undefined"
        assert memory["id"] not in nodes, "memory must not alter ordinary node inventory"
        assert len(memory["portIds"]) == 2
        for identifier in memory["portIds"]:
            assert nodes[identifier]["parentId"] == memory["id"]
    external = [node for node in nodes.values() if node["type"] == "NODE_EXT"]
    assert len(external) == 1
    ext = external[0]
    assert ext["externalDefname"] == "Echo"
    assert {nodes[identifier]["name"] for identifier in ext["memberIds"]} == {"ext$data", "ext$result"}
    assert nodes[ext["clockId"]]["name"] == "ext$clock"
    assert {nodes[identifier]["name"] for identifier in ext["boundaryPortIds"]} == {
        "ext$clock", "ext$secondaryClock", "ext$data", "ext$result"}
    secondary = [nodes[identifier] for identifier in ext["boundaryPortIds"]
                 if nodes[identifier]["name"] == "ext$secondaryClock"]
    assert len(secondary) == 1 and secondary[0]["type"] == "NODE_OTHERS"
    assert all(nodes[identifier]["parentId"] == ext["id"] for identifier in ext["memberIds"])
    print(json.dumps({"memories": len(memories), "external": len(external), "ordinary_nodes": len(nodes)}))


if __name__ == "__main__":
    main()
