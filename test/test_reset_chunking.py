"""Check reset chunking and register updates across generated source files.

Run after make build-gsim: python3 test/test_reset_chunking.py
"""

import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
GSIM = ROOT / "build/gsim/gsim"


def run(command, env):
    result = subprocess.run(list(map(str, command)), env=env, text=True,
                            capture_output=True, timeout=90)
    if result.returncode:
        raise AssertionError(f"{command} failed:\n{result.stdout}\n{result.stderr}")
    return result


class ResetChunkingTest(unittest.TestCase):
    def test_reset_then_register_updates(self):
        # Cover both the original small reset path and resets spanning chunks.
        # Distinct reset values prevent common-expression and alias passes from
        # merging the registers whose values we check after reset is released.
        for reset_type in ("UInt<1>", "AsyncReset"):
            for register_count in (16, 1200):
                with self.subTest(reset=reset_type, registers=register_count):
                    self.check_reset(reset_type, register_count)

    def check_reset(self, reset_type, register_count):
        fir = ["FIRRTL version 3.3.0", "circuit ResetChunks :",
               "  module ResetChunks :", "    input clock : Clock",
               f"    input reset : {reset_type}", "    input data : UInt<16>"]
        fir.extend(f"    output out{i} : UInt<16>" for i in range(register_count))
        for i in range(register_count):
            fir.extend([f"    regreset r{i} : UInt<16>, clock, reset, UInt<16>({i + 1})",
                        f"    connect r{i}, data", f"    connect out{i}, r{i}"])
        harness = [
            '#include "ResetChunks.h"', '#include <cstdio>',
            "int main() { SResetChunks dut; unsigned previous = 0;",
            "for (unsigned cycle = 0; cycle < 40; ++cycle) {",
            ("const bool reset = cycle == 0 || cycle == 17 || cycle == 18;"
             if reset_type == "UInt<1>" else "const bool reset = cycle == 0;"),
            "const unsigned data = cycle * 31 + 7;",
            "dut.set_clock(cycle & 1); dut.set_reset(reset); dut.set_data(data);",
            "dut.step();",
        ]
        for i in range(register_count):
            harness.append(
                f"if (dut.get_out{i}() != (reset ? {i + 1} : previous)) {{"
                f' std::fprintf(stderr, "cycle %u register {i}: %u\\n", cycle,'
                f" static_cast<unsigned>(dut.get_out{i}())); return 1; }}")
        harness.append('previous = data; } std::puts("ok"); }')

        with tempfile.TemporaryDirectory(prefix="gsim-reset-chunks-") as directory:
            out = Path(directory)
            design = out / "design.fir"
            design.write_text("\n".join(fir) + "\n")
            main = out / "main.cpp"
            main.write_text("\n".join(harness) + "\n")
            env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
            run([GSIM, "--cpp-max-size-KB=8", "--dir", out, design], env)
            sources = sorted(out.glob("ResetChunks*.cpp"))
            # Confirm that large reset bodies really span functions and files;
            # runtime checks verify initialization and subsequent updates;
            # the synchronous case also asserts reset again during execution.
            chunk_files = [source for source in sources if re.search(
                r"void SResetChunks::subReset\d+Chunk\d+\(\)", source.read_text())]
            if register_count == 1200:
                self.assertGreater(len(chunk_files), 1)
            else:
                self.assertEqual(chunk_files, [])
            run(["clang++", "-std=c++17", "-O1", "-pthread",
                 "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                 "-I" + str(out), *sources, main, "-o", out / "emu"], env)
            self.assertEqual(run([out / "emu"], env).stdout.strip(), "ok")


if __name__ == "__main__":
    unittest.main()
