"""Exercise public writer lifecycle rejection without parsing a circuit."""
import argparse
import json
from pathlib import Path
import subprocess


def check(binary, work):
    work.mkdir(parents=True, exist_ok=True)
    invalid = ("invalid-stage", "null-stage", "after-before", "event-before", "finish-before",
               "duplicate-before", "finish-without-after", "duplicate-after", "event-after",
               "duplicate-finish", "inventory-after-finish", "event-after-finish")
    for mode in ("valid", *invalid):
        output = work / f"{mode}.jsonl"
        result = subprocess.run([str(binary), mode, str(output)], capture_output=True, text=True)
        (work / f"{mode}.log").write_text(result.stdout + result.stderr)
        records = [json.loads(line) for line in output.read_text().splitlines()]
        if mode == "valid":
            assert result.returncode == 0, result.stderr
            assert records[-1] == {"record": "footer", "complete": True, "beforeCount": 0,
                                   "afterCount": 0, "eventCount": 0, "mutationCount": 0,
                                   "replayComplete": True}
        else:
            assert result.returncode == 1, (mode, result.returncode, result.stderr)
            assert "Invalid native node evolution lifecycle:" in result.stderr, (mode, result.stderr)
        assert sum(record["record"] == "footer" for record in records) <= 1, mode
    print(json.dumps({"valid_lifecycle": 1, "rejected_invalid_lifecycles": len(invalid)}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    check(args.binary.resolve(), args.work.resolve())
