"""Check batched 64-bit commits and the unchanged per-register fallback."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class RepCutBulkCommitTest(unittest.TestCase):
    def test_commit_copy(self):
        for design, workers in (("repcut-bulk-commit", 1),
                                ("repcut-bulk-commit", 2),
                                ("repcut-bulk-distinct", 1),
                                ("repcut-bulk-distinct", 2)):
            with self.subTest(design=design, workers=workers), tempfile.TemporaryDirectory() as directory:
                output = Path(directory)
                env = dict(os.environ, GSIM_THREADS=str(workers), ASAN_OPTIONS="detect_leaks=0")
                subprocess.run([ROOT / "build/gsim/gsim", "--mt-mode=repcut",
                                "--supernode-max-size=30", "--dir", output,
                                ROOT / "test" / (design + ".fir")], env=env,
                               capture_output=True, text=True, check=True, timeout=90)
                sources = sorted(output.glob("RcBulkCommit*.cpp"))
                generated = "\n".join(source.read_text() for source in sources)
                self.assertNotIn("rcBulkOld", generated)
                if design == "repcut-bulk-commit" and workers == 1:
                    compare = generated.index("memcmp((const char*)this + offsetof(SRcBulkCommit, r1)")
                    copy = generated.index("memcpy((char*)this + offsetof(SRcBulkCommit, r1)")
                    self.assertLess(compare, copy)
                    self.assertEqual(generated.count("memcpy((char*)this + offsetof(SRcBulkCommit, r1)"), 1)
                elif design == "repcut-bulk-distinct" and workers == 1:
                    compare = generated.index("if (r2$NEXT != r2)")
                    copy = generated.index("memcpy((char*)this + offsetof(SRcBulkCommit, r2)")
                    self.assertLess(compare, copy)
                    self.assertEqual(generated.count("memcpy((char*)this + offsetof(SRcBulkCommit, r2)"), 1)
                else:
                    self.assertIn("uint64_t r1$old$", generated)
                    if design == "repcut-bulk-distinct":
                        self.assertIn("rcCommitPend", generated)
                for logging in (False, True):
                    binary = output / ("emu-log" if logging else "emu")
                    subprocess.run(["clang++", "-std=c++17", "-O1", "-pthread",
                                    "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                                    *(["-DENABLE_LOG"] if logging else []),
                                    "-I" + str(output), *sources,
                                    ROOT / "test" / (design + ".cpp"), "-o", binary],
                                   capture_output=True, text=True, check=True, timeout=90)
                    result = subprocess.run([binary], env=env, capture_output=True,
                                            text=True, check=True, timeout=90)
                    self.assertEqual(result.stdout.strip(), "ok")


if __name__ == "__main__":
    unittest.main()
