"""Verify native all-node replay and fail closed when change provenance is absent."""
import argparse
import json
from pathlib import Path
import subprocess


def key(ref):
    return ref["kind"], ref["id"], ref["generation"]


def verify(path):
    rows = [json.loads(line) for line in path.read_text().splitlines()]
    assert rows[0]["replayContract"] == "native-field-delta.v1"
    assert rows[-1]["complete"] and rows[-1]["replayComplete"]
    before, oracle, events, mutations = {}, {}, {}, []
    for row in rows[1:-1]:
        if row["record"] == "entity":
            target = before if row["stage"] == "before" else oracle
            target[key(row["ref"])] = {name: value for name, value in row.items()
                                      if name not in ("record", "stage", "ref")}
        elif row["record"] == "event":
            events[row["seq"]] = row
        elif row["record"] == "mutation":
            mutations.append(row)
        else:
            raise AssertionError(row["record"])
    # Oracle bodies never provide replay payloads.
    replayed = dict(before)
    for seq, change in enumerate(mutations, 1):
        assert change["seq"] == seq
        entity = key(change["ref"])
        event = events[change["eventSeq"]]
        assert entity in {key(ref) for ref in event["before"] + event["after"]}
        if change["action"] == "create":
            assert entity not in replayed and not change["expect"] and not change["unset"]
            replayed[entity] = change["set"]
        elif change["action"] == "erase":
            assert replayed.pop(entity) == change["expect"]
        else:
            current = dict(replayed[entity])
            assert change["action"] == "update"
            assert {name: current[name] for name in change["expect"]} == change["expect"]
            for name in change["unset"]:
                del current[name]
            current.update(change["set"])
            replayed[entity] = current
    assert replayed == oracle, path
    for inventory in (before, replayed):
        shared = {}
        for body in inventory.values():
            structure = body["structure"]
            expressions = {row["id"]: row for row in structure["expressions"]}
            for expression in expressions.values():
                assert all(child is None or child in expressions for child in expression["children"])
                previous = shared.setdefault(expression["id"], expression)
                assert previous == expression, "shared ENode owners disagree"
            for tree in [*structure["assignTrees"], structure["resetTree"]]:
                if tree:
                    assert all(tree[field] is None or tree[field] in expressions
                               for field in ("lvalue", "root"))
    return {"before": len(before), "after": len(oracle), "mutations": len(mutations)}


def check(binary, producer, work):
    work.mkdir(parents=True, exist_ok=True)
    results = {}
    invalid = ("missing-scalar-event", "missing-shared-owner-event", "retain-cannot-explain-mutation")
    for mode in ("valid-shared-change", *invalid):
        trace = work / f"{mode}.jsonl"
        result = subprocess.run([str(binary), mode, str(trace)], capture_output=True, text=True)
        (work / f"{mode}.log").write_text(result.stdout + result.stderr)
        if mode == "valid-shared-change":
            assert result.returncode == 0, result.stderr
            results[mode] = verify(trace)
        else:
            assert result.returncode == 1, (mode, result.stderr)
            message = "only to a retain event" if mode == "retain-cannot-explain-mutation" else "no recorded evolution event"
            assert message in result.stderr, result.stderr
            assert not any(json.loads(row).get("complete") for row in trace.read_text().splitlines())
    fixtures = Path(__file__).resolve().parents[1] / "test"
    for name in ("dump-semantic-lineage", "dump-memory-inventory", "dump-source-aggregates"):
        trace = work / f"{name}.jsonl"
        result = subprocess.run([str(producer), "--stop-after-stage=SplitArray",
                                 f"--trace-node-evolution={trace}", str(fixtures / f"{name}.fir")],
                                capture_output=True, text=True)
        (work / f"{name}.log").write_text(result.stdout + result.stderr)
        assert result.returncode == 0, result.stderr
        results[name] = verify(trace)
    print(json.dumps({"exact_replay": results, "rejected_missing_or_invalid_native_events": len(invalid)}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--producer", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    check(args.binary.resolve(), args.producer.resolve(), args.work.resolve())
