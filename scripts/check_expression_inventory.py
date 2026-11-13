"""Check faithful sidecar slots, references, selection, and explicit truncation."""
import argparse
import filecmp
import json
from pathlib import Path


def read(path):
    rows = [json.loads(line) for line in path.read_text().splitlines()]
    assert rows[0]["record"] == "header" and rows[0]["schema"] == "gsim.expressions.v1"
    assert rows[-1]["record"] == "footer" and rows[-1]["complete"]
    nodes = {row["id"]: row for row in rows if row["record"] == "node"}
    assert len(nodes) == rows[-1]["nodeCount"]
    for row in nodes.values():
        if not row["treeInventoryComplete"]:
            assert row["closureBoundary"] and row["trees"] is None
            continue
        assert set(row["trees"]) == {"assignTree", "valTree", "resetTree", "resetCond", "resetVal", "memTree"}
        trees = row["trees"]
        for tree in trees["assignTree"] + [trees[slot] for slot in trees if slot != "assignTree"]:
            if tree is None:
                continue
            expressions = {entry["id"]: entry for entry in tree["nodes"]}
            assert len(expressions) == len(tree["nodes"])
            assert tree["root"] is None or tree["root"] in expressions
            assert tree["lvalue"] is None or tree["lvalue"] in expressions
            for expression in expressions.values():
                for child in expression["children"]:
                    assert child is None or child in expressions
                assert isinstance(expression["opcode"], int)
                assert isinstance(expression["nativeId"], int)
                assert all(key in expression for key in ("nodeId", "memoryNodeId", "values", "strVal", "usedBit"))
                if rows[-1]["closureComplete"] and not row["closureBoundary"]:
                    for key in ("nodeId", "memoryNodeId"):
                        assert expression[key] is None or expression[key] in nodes
    for use in (row for row in rows if row["record"] == "seed-use"):
        assert use["seedId"] in rows[0]["selectedSeedIds"]
        consumer = nodes[use["consumerId"]]
        tree = consumer["trees"][use["slot"]]
        if use["slot"] == "assignTree":
            tree = tree[use["treeIndex"]]
        assert any(expression["nativeId"] == use["nativeExpressionId"] and expression["nodeId"] == use["seedId"]
                   for expression in tree["nodes"])
    return rows, nodes


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    native = []
    for index, stage in enumerate(("Init", "TopoSort", "InferAllWidth")):
        rows, nodes = read(args.work / f"DumpExpressionInventory_{index}{stage}_Expressions.jsonl")
        assert rows[-1]["closureComplete"] and not rows[-1]["truncationReasons"]
        kinds = {node["sourceKind"] for node in nodes.values() if node["seed"]}
        assert kinds == {"when-condition", "memory-read-address"}
        assert rows[-1]["consumerCount"] > 0
        for node in nodes.values():
            if node["sourceKind"] == "memory-read-address":
                assert node["clockId"] in nodes and node["regNextId"] in nodes
                assert node["sourceOwnerId"] in nodes
                owner = nodes[node["sourceOwnerId"]]
                assert owner["parentId"] in nodes
                assert nodes[owner["parentId"]]["type"] == "NODE_MEMORY"
        native.append({"stage": stage, "nodes": len(nodes), "seeds": rows[-1]["seedCount"]})
        assert filecmp.cmp(args.work / f"DumpExpressionInventory_{index}{stage}.json",
                           args.work / "without-sidecar" / f"DumpExpressionInventory_{index}{stage}.json", shallow=False)
    rows, nodes = read(args.work / "SyntheticExpressions_Synthetic_Expressions.jsonl")
    assert rows[-1]["closureComplete"]
    by_name = {node["name"]: node for node in nodes.values()}
    assert by_name["condition"]["trees"]["assignTree"][0] is None
    assert by_name["reader"]["trees"]["valTree"]["root"] is None
    for name, missing in (("left", 2), ("right", 1)):
        tree = by_name[name]["trees"]["assignTree"][0]
        root = tree["nodes"][tree["root"]]
        assert root["op"] == "OP_WHEN" and len(root["children"]) == 3
        assert root["children"][missing] is None and root["children"][3 - missing] is not None
    right_tree = by_name["right"]["trees"]["assignTree"][0]
    unknown = next(node for node in right_tree["nodes"] if node["op"] == "OP_UNKNOWN")
    assert unknown["opcode"] == 63 and unknown["width"] == -1
    assert unknown["values"] == [3, -2] and unknown["strVal"] == 'raw\nunknown"payload'
    assert by_name["address"]["trees"]["resetTree"] is not None
    assert by_name["address"]["trees"]["resetCond"] is not None
    assert by_name["address"]["trees"]["resetVal"] is not None
    assert by_name["input"]["closureBoundary"]
    assert by_name["memory"]["closureBoundary"] and by_name["memory"]["depth"] == 4
    cuts, cut_nodes = read(args.work / "SyntheticExpressions_SourceCuts_Expressions.jsonl")
    assert cuts[-1]["closureComplete"]
    cut_names = {node["name"]: node for node in cut_nodes.values()}
    assert cut_names["intermediate"]["boundaryReason"] == "source-cut-candidate"
    assert cut_names["intermediate"]["trees"] is None and not cut_names["intermediate"]["treeInventoryComplete"]
    assert cut_names["address"]["treeInventoryComplete"] and cut_names["addressNext"]["treeInventoryComplete"]
    assert cut_names["reader"]["treeInventoryComplete"] and cut_names["reader"]["trees"]["memTree"]
    capped, _ = read(args.work / "SyntheticExpressions_Capped_Expressions.jsonl")
    assert not capped[-1]["closureComplete"] and "node-cap" in capped[-1]["truncationReasons"]
    filtered, filtered_nodes = read(args.work / "SyntheticExpressions_IdFiltered_Expressions.jsonl")
    assert filtered[-1]["closureComplete"] and filtered[-1]["seedCount"] == 1
    assert {node["name"] for node in filtered_nodes.values()} == {"input", "condition", "left", "right"}
    missing, _ = read(args.work / "SyntheticExpressions_MissingSeed_Expressions.jsonl")
    assert not missing[-1]["closureComplete"] and "requested-seed-missing" in missing[-1]["truncationReasons"]
    print(json.dumps({"native": native, "synthetic_nodes": len(nodes), "null_slots": True,
                      "numeric_references": True, "unknown_opcode": True, "cap_reported": True,
                      "id_filter": True, "ordinary_json_identical": True, "source_cuts": True}))


if __name__ == "__main__":
    main()
