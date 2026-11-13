"""Check stable source aggregate identities and explicit register pair links."""
import argparse
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    graphs = [json.loads((args.work / ("DumpSourceAggregates_" + stage + ".json")).read_text())
              for stage in ("0Init", "1TopoSort", "2InferAllWidth")]
    initial = graphs[0]
    initial_nodes = {node["id"]: node for node in initial["nodes"]}
    initial_entities = {**initial_nodes, **{node["id"]: node for node in initial["memories"]}}
    assert all(graph["sourceReplacementInventoryComplete"] is True for graph in graphs)
    replacements = initial["sourceReplacements"]
    assert all(graph["sourceReplacements"] == replacements for graph in graphs)
    replacement_map = {item["originalId"]: item for item in replacements}
    assert len(replacements) == len(replacement_map) == 2
    assert len({item["replacementId"] for item in replacements}) == len(replacements)
    assert not set(replacement_map) & {item["replacementId"] for item in replacements}
    for replacement in replacements:
        assert replacement["reason"] == "depth-one-memory-to-register"
        assert replacement["originalDepth"] == 1 and replacement["originalDimension"] == []
        assert replacement["originalId"] not in initial_entities
        actual = initial_nodes[replacement["replacementId"]]
        assert actual["type"] == "NODE_REG_SRC"
        assert replacement["originalName"] == actual["name"]
        assert replacement["originalWidth"] == actual["width"]
        assert replacement["originalSign"] == bool(actual["sign"])
        assert actual["semantic"]["role"] == "register-current"
        assert actual["semantic"]["originRole"] == "memory-storage"
        assert actual["semantic"]["storageDepth"] == 1
    aggregates = initial["sourceAggregates"]
    assert all(graph["sourceAggregateInventoryComplete"] is True for graph in graphs)
    assert all(graph["sourceScopeInventoryComplete"] is True for graph in graphs)
    assert all(graph["sourceAggregates"] == aggregates for graph in graphs)
    assert all(graph["sourceScopes"] == initial["sourceScopes"] for graph in graphs)
    scopes = initial["sourceScopes"]
    assert initial["semanticLineageVersion"] == 1
    assert len(scopes) == 2
    assert scopes[0]["path"] == "" and scopes[0]["module"] == "DumpSourceAggregates"
    assert scopes[0]["instanceLine"] == -1 and scopes[0]["kind"] == "module"
    assert scopes[0]["semanticPath"] == []
    assert scopes[1]["path"] == "child" and scopes[1]["module"] == "Child"
    assert scopes[1]["line"] > 0 and scopes[1]["instanceLine"] > scopes[1]["line"]
    assert scopes[1]["semanticPath"] == ["child"]
    assert [item["name"] for item in aggregates] == sorted(item["name"] for item in aggregates)
    by_name = {item["name"]: item for item in aggregates}
    by_id = {item["id"]: item for item in aggregates}
    assert len(by_id) == len(by_name) == len(aggregates) == 16
    assert set(by_name) == {"bus", "bus__DOT__nested", "bundle", "state", "state__DOT__nested",
                            "records", "records__DOT__nested", "singleton", "singleton__DOT__nested",
                            "sw", "sw__DOT__nested", "sr", "sr__DOT__nested", "alias", "alias__DOT__nested", "nestedAlias"}
    assert by_name["bus"]["nestedAggregateIds"] == [by_name["bus__DOT__nested"]["id"]]
    assert by_name["state"]["nestedAggregateIds"] == [by_name["state__DOT__nested"]["id"]]
    for aggregate in aggregates:
        assert aggregate["semanticPath"]
        assert len(aggregate["memberIds"]) == len(aggregate["memberFlips"])
        assert len(aggregate["memberIds"]) == len(aggregate["originalMemberIds"])
        for original, member in zip(aggregate["originalMemberIds"], aggregate["memberIds"]):
            if original != member:
                assert replacement_map[original]["replacementId"] == member
        assert all(identifier in initial_entities for identifier in aggregate["memberIds"])
        assert all(identifier in by_id for identifier in aggregate["nestedAggregateIds"])
    assert by_name["records"]["nestedAggregateIds"] == [by_name["records__DOT__nested"]["id"]]
    assert by_name["records__DOT__nested"]["semanticPath"] == ["records", "nested"]
    assert {initial_entities[identifier]["name"] for identifier in by_name["records"]["memberIds"]} == {
        "records__DOT__data", "records__DOT__nested__DOT__tag"}
    assert all(initial_entities[identifier]["type"] == "NODE_MEMORY"
               for identifier in by_name["records"]["memberIds"])
    assert set(by_name["singleton"]["originalMemberIds"]) == set(replacement_map)
    assert set(by_name["singleton"]["memberIds"]) == {item["replacementId"] for item in replacements}
    for name in ("singleton", "sw", "sr", "alias"):
        assert by_name[name]["nestedAggregateIds"] == [by_name[name + "__DOT__nested"]["id"]]
    assert len(by_name["nestedAlias"]["memberIds"]) == 1
    assert by_name["nestedAlias"]["semanticPath"] == ["nestedAlias"]
    bus_flips = {initial_nodes[identifier]["name"]: flip for identifier, flip
                 in zip(by_name["bus"]["memberIds"], by_name["bus"]["memberFlips"])}
    assert bus_flips == {"bus__DOT__a": False, "bus__DOT__b": True,
                         "bus__DOT__nested__DOT__c": False, "bus__DOT__nested__DOT__d": True}
    for graph in graphs:
        nodes = {node["id"]: node for node in graph["nodes"]}
        generated = [node for node in nodes.values() if "sourceKind" in node]
        conditions = [node for node in generated if node["sourceKind"] == "when-condition"]
        addresses = [node for node in generated if node["sourceKind"] == "memory-read-address"]
        assert len(conditions) == 1 and conditions[0]["sourceLine"] > 0
        assert conditions[0]["sourceOwnerId"] == -1
        assert len(addresses) == 2
        for address in addresses:
            owner = initial_nodes[address["sourceOwnerId"]]
            assert owner["name"] == "reader" and owner["type"] == "NODE_READER"
            assert address["sourceLine"] == owner["lineno"]
            assert address["semantic"]["path"] == ["reader"]
            assert address["semantic"]["role"] == "memory-read-address"
            assert address["semantic"]["endpoint"] == ("current" if address["type"] == "NODE_REG_SRC" else "next")
        for condition in conditions:
            assert condition["semantic"]["path"] == ["when@" + str(condition["sourceLine"])]
            assert condition["semantic"]["role"] == "predicate"
        for register in nodes.values():
            if register["type"] not in ("NODE_REG_SRC", "NODE_REG_DST"):
                assert all(key not in register for key in ("regNextId", "regSourceId", "regDestinationId"))
                continue
            source, destination = nodes[register["regSourceId"]], nodes[register["regDestinationId"]]
            assert source["type"] == "NODE_REG_SRC" and destination["type"] == "NODE_REG_DST"
            assert source["regNextId"] == destination["id"]
            assert destination["regNextId"] == source["id"]
            assert source["regSourceId"] == destination["regSourceId"] == source["id"]
            assert source["regDestinationId"] == destination["regDestinationId"] == destination["id"]
    live_nodes = {node["id"]: node for node in graphs[1]["nodes"]}
    original = next(node for node in initial_nodes.values() if node["name"] == "bundle__DOT__a_0")
    assert live_nodes[original["id"]]["name"] == "bundle__DOT__a_0_0"
    assert live_nodes[original["id"]]["semantic"]["path"] == ["bundle", "a_0"]
    assert original["id"] in by_name["bundle"]["memberIds"]
    split = json.loads((args.work / "split-array-map.json").read_text())
    original_array = next(node for node in initial_nodes.values() if node["name"] == "state__DOT__x")
    rows = [row for row in split["entries"] if row["parentId"] == original_array["id"]]
    assert len(rows) == 2
    assert original_array["id"] in by_name["state"]["memberIds"]
    assert all(live_nodes[row["memberId"]]["type"] == "NODE_REG_SRC" for row in rows)
    print(json.dumps({"source_aggregates": len(aggregates), "stable_stages": len(graphs),
                      "source_scopes": len(scopes), "generated_conditions": len(conditions),
                      "address_register_nodes": len(addresses),
                      "depth_one_memory_replacements": len(replacements),
                      "split_register_members": len(rows), "renamed_source_id_preserved": True}))


if __name__ == "__main__":
    main()
