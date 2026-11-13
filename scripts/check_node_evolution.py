"""Check native splitArray evolution, reason coverage, and diagnostic transparency."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path


def ref_id(ref):
    assert ref["kind"] == "gsim-node"
    assert ref["generation"] == 0
    return ref["id"]


def check(work):
    records = [json.loads(line) for line in (work / "evolution.jsonl").read_text().splitlines()]
    header, footer = records[0], records[-1]
    assert header["schema"] == "wolvrix.node-evolution.v1"
    assert header["engine"] == "gsim" and header["pass"] == "splitArray"
    assert header["graph"] == "NodeEvolution"
    assert header["inventoryScope"] == "all-live" and header["implicitIdentity"]
    assert header["replayContract"] == "native-field-delta.v1"
    fixture = Path(__file__).resolve().parents[1] / "test" / "node-evolution.fir"
    assert header["inputIdentity"] == {"sha256": hashlib.sha256(fixture.read_bytes()).hexdigest(),
                                        "authority": "caller"}
    assert footer["record"] == "footer" and footer["complete"]
    entities = {stage: {} for stage in ("before", "after")}
    events = []
    mutations = []
    for row in records[1:-1]:
        if row["record"] == "event":
            events.append(row)
        elif row["record"] == "mutation":
            mutations.append(row)
        else:
            assert row["record"] == "entity"
            sid = ref_id(row["ref"])
            assert sid not in entities[row["stage"]]
            entities[row["stage"]][sid] = row
            source = row["sources"][0]
            assert len(source["shape"]) == len(source["projection"]) == len(source["shapeFieldDepths"])
            assert source["width"] == row["physical"]["width"]
    before, after = entities["before"], entities["after"]
    assert footer["beforeCount"] == len(before) and footer["afterCount"] == len(after)
    assert footer["eventCount"] == len(events)
    assert footer["mutationCount"] == len(mutations) and footer["replayComplete"]
    # Replay only before bodies plus mutation payloads. Actual after bodies are
    # read exclusively after the reconstructed map is complete.
    def body(row):
        return {key: value for key, value in row.items() if key not in ("record", "stage", "ref")}
    replayed = {sid: body(row) for sid, row in before.items()}
    for expected_seq, change in enumerate(mutations, 1):
        assert change["seq"] == expected_seq
        sid = ref_id(change["ref"])
        event = events[change["eventSeq"] - 1]
        assert sid in {ref_id(ref) for ref in event["before"] + event["after"]}
        if change["action"] == "create":
            assert sid not in replayed and not change["expect"] and not change["unset"]
            replayed[sid] = change["set"]
        elif change["action"] == "erase":
            assert replayed.pop(sid) == change["expect"]
            assert not change["set"] and not change["unset"]
        else:
            assert change["action"] == "update"
            current = replayed[sid]
            assert {key: current[key] for key in change["expect"]} == change["expect"]
            assert all(key in change["expect"] or key not in current for key in change["set"])
            for key in change["unset"]:
                del current[key]
            current.update(change["set"])
    assert replayed == {sid: body(row) for sid, row in after.items()}
    assert any(before[ref_id(row["ref"])]["physical"]["shape"] == []
               for row in mutations if row["action"] == "update"), "scalar mutation replay missing"
    assert any(row["opKind"] == 6 for row in before.values()), "memory replay fixture missing"
    assert [e["seq"] for e in events] == list(range(1, len(events) + 1))
    all_ids = before.keys() | after.keys()
    dispositions, creations = Counter(), Counter()
    for event in events:
        for ref in event["before"]:
            sid = ref_id(ref)
            assert sid in before
            dispositions[sid] += 1
        for ref in event["after"]:
            sid = ref_id(ref)
            assert sid in after
            creations[sid] += 1
        assert event["steps"]
    assert all(count == 1 for count in dispositions.values())
    assert all(count == 1 for count in creations.values())
    steps = sorted((step for event in events for step in event["steps"]), key=lambda row: row["seq"])
    assert [step["seq"] for step in steps] == list(range(1, len(steps) + 1))
    reasons = Counter(e["reason"] for e in steps)
    for reason in ("direct_array_self_dependency", "static_indexed_array_access",
                   "paired_register_array_split", "register_member_pair_bound",
                   "split_array_reference_rewrite", "split_array_shared_expression_rewrite", "dynamic_write_index",
                   "dynamic_consumer_index", "unsupported_array_node_kind",
                   "unsplit_aggregate_input"):
        assert reasons[reason], (reason, reasons)
    split_parents, members = set(), set()
    for event in steps:
        if event["action"] != "split":
            continue
        assert event["geometry"]["partition"]
        parent = ref_id(event["before"][0])
        split_parents.add(parent)
        assert parent in before and parent not in after
        expected = before[parent]["sources"][0]
        for ref in event["after"]:
            sid = ref_id(ref)
            members.add(sid)
            source = after[sid]["sources"][0]
            assert source["path"] == expected["path"]
            assert source["shape"] == expected["shape"]
            assert all(i >= 0 for i in source["projection"])
        assert len(event["after"]) == event["geometry"]["memberCount"]
    assert before.keys() - after.keys() == split_parents
    assert after.keys() - before.keys() == members
    changed = {sid for sid in before.keys() & after.keys()
               if before[sid]["structure"] != after[sid]["structure"]}
    assert changed, "native expression rewrites are invisible in inventories"
    assert changed <= dispositions.keys() and changed <= creations.keys()
    shared_paths = {("reader", field) for field in ("a", "b")}
    shared_owners = {sid for sid, entity in before.items()
                     if tuple(entity["sources"][0]["path"]) in shared_paths}
    assert len(shared_owners) == 2, shared_owners
    assert shared_owners <= changed, "shared memory addresses must rewrite every physical owner"
    assert before.keys() - after.keys() <= dispositions.keys()
    assert after.keys() - before.keys() <= creations.keys()
    assert any(len(event["steps"]) > 1 for event in events)
    for event in events:
        if len(event["steps"]) > 1:
            assert not event["geometry"]["partition"]
    for step in steps:
        if step["reason"] == "split_array_reference_rewrite":
            sid = ref_id(step["before"][0])
            if sid in before and sid in after:
                assert sid in changed
    for sid, entity in after.items():
        if "registerPair" in entity:
            pair = ref_id(entity["registerPair"])
            assert ref_id(after[pair]["registerPair"]) == sid
    for stage in ("0Init", "1TopoSort"):
        assert (work / f"NodeEvolution_{stage}.json").read_bytes() == (
            work / "plain" / f"NodeEvolution_{stage}.json").read_bytes()
    assert not (work / "plain" / "evolution.jsonl").exists()
    print(json.dumps({"before": len(before), "after": len(after), "events": len(events),
                      "native_steps": len(steps), "mutations": len(mutations), "replay_exact": True,
                      "changed_structures": len(changed), "reasons": reasons}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", type=Path, required=True)
    check(parser.parse_args().work)
