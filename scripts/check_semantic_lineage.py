"""Check native semantic lineage at the live post-split checkpoint."""
import argparse
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    stages = [json.loads((args.work / ("DumpSemanticLineage_" + stage + ".json")).read_text())
              for stage in ("0Init", "1TopoSort", "2InferAllWidth")]
    initial = {node["id"]: node for node in stages[0]["nodes"]}
    for graph in stages:
        for node in graph["nodes"]:
            semantic = node["semantic"]
            assert semantic["path"] and all(semantic["path"])
            assert 0 <= semantic["scopeDepth"] < len(semantic["path"])
            assert len(semantic["shape"]) == len(semantic["shapeFieldDepths"]) == len(semantic["indices"])
            for index, extent in zip(semantic["indices"], semantic["shape"]):
                assert -1 <= index < extent
            if node["id"] in initial:
                assert semantic == initial[node["id"]]["semantic"]
    live = stages[1]["nodes"]
    renamed = next(node for node in live if node["semantic"]["path"] == ["bundle", "data_0"])
    assert renamed["name"].endswith("data_0_0")
    split_data = [node for node in live if node["semantic"]["path"] == ["bundle", "data"]]
    assert {tuple(node["semantic"]["indices"]) for node in split_data} == {(0,), (1,)}
    for node in split_data:
        assert node["semantic"]["shape"] == [2]
        assert node["semantic"]["shapeFieldDepths"] == [1]
    input_data = [node for node in live if node["semantic"]["path"] == ["child", "values", "data"]]
    assert input_data
    for node in input_data:
        assert node["semantic"]["scopeDepth"] == 1
        assert node["semantic"]["shape"] == [1, 2]
        assert node["semantic"]["shapeFieldDepths"] == [0, 1]
        assert node["semantic"]["indices"][0] == 0
    state = [node for node in live if node["semantic"]["path"] == ["child", "state"]]
    assert len(state) == 4
    assert {(node["semantic"]["role"], tuple(node["semantic"]["indices"])) for node in state} == {
        (role, (index, 0)) for role in ("register-current", "register-next") for index in (0, 1)}
    for node in state:
        assert node["semantic"]["shape"] == [2, 1]
        assert node["semantic"]["shapeFieldDepths"] == [0, 0]
    condition = next(node for node in live if node["semantic"]["role"] == "predicate")
    assert condition["semantic"]["path"] == ["child", "when@10"]
    effects = [node for node in live if node["semantic"]["role"] == "effect"]
    assert {node["semantic"]["statementKind"] for node in effects} == {"printf", "assert", "stop"}
    for node in effects:
        assert node["semantic"]["path"] == ["effect@" + str(node["semantic"]["line"])]
    assert {tuple(node["semantic"]["path"]) for node in effects} == {
        ("effect@25",), ("effect@26",), ("effect@27",)}
    print(json.dumps({"native_path_stages": len(stages), "split_state_endpoints": len(state),
                      "singleton_and_interleaved_geometry": True, "renamed_source_preserved": True}))


if __name__ == "__main__":
    main()
