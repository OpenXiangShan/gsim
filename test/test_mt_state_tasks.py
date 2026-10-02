"""Verify state-task ownership, register timing, reset and pending commits.

Run after `make build-gsim`: python3 test/test_mt_state_tasks.py
"""
import csv
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
GSIM = ROOT / 'build/gsim/gsim'


def run(command, env):
    result = subprocess.run(list(map(str, command)), env=env, text=True,
                            capture_output=True, timeout=90)
    if result.returncode:
        raise AssertionError(f'{command} failed:\n{result.stdout}\n{result.stderr}')
    return result


class StateTasksTest(unittest.TestCase):
    def generate(self, out, design, workers, scheduler='heft', lookahead=0, extra=()):
        env = dict(os.environ, GSIM_THREADS=str(workers), GSIM_MT_EXECUTOR='dense')
        run([GSIM, '--mt-mode=on', '--mt-target-tasks=64',
             '--mt-scheduler=' + scheduler, f'--mt-lookahead-window={lookahead}',
             *extra, '--dir', out, ROOT / 'test' / f'{design}.fir'], env)
        with (out / 'mt-task-phases.csv').open() as stream:
            phases = list(csv.DictReader(stream))
        with (out / 'mt-state-task-members.csv').open() as stream:
            members = list(csv.DictReader(stream))
        states = [r for r in phases if r['phase'] == 'state']
        compute = [r for r in phases if r['phase'] == 'compute']
        owned = [r for r in members if r['role'] == 'member']
        self.assertEqual(len(states), workers)
        self.assertEqual({int(r['worker']) for r in states}, set(range(workers)))
        self.assertTrue(all(r['task'] == r['worker'] for r in states))
        self.assertTrue(all(r['barrier'] == 'state_publish' for r in states))
        self.assertTrue(all(r['reg_src_members'] == '0' and
                            r['barrier'] == 'state_acquire' for r in compute))
        self.assertEqual(len({r['node_id'] for r in owned}), len(owned))
        self.assertEqual(sum(int(r['members']) for r in states), len(owned))
        self.assertEqual(sum(int(r['reg_src_members']) for r in states),
                         sum(r['node_type'] == 'reg_src' for r in owned))
        return env, members

    def simulate(self, out, design, env):
        exe = out / 'emu-test'
        run(['clang++', '-std=c++17', '-O1', '-Wno-parentheses-equality',
             '-I' + str(out), *sorted(out.glob('*.cpp')),
             ROOT / 'test' / f'{design}.cpp', '-pthread', '-o', exe], env)
        return run([exe], env).stdout.strip()

    def test_registers_and_reset_against_reference(self):
        signatures = []
        for workers in (1, 4):
            for scheduler in ('heft', 'list'):
                for lookahead in (0, 16):
                    with self.subTest(workers=workers, scheduler=scheduler,
                                      lookahead=lookahead), tempfile.TemporaryDirectory(
                                          prefix='mt-state-tasks-') as directory:
                        out = Path(directory)
                        env, members = self.generate(out, 'mt-state-tasks', workers,
                                                     scheduler, lookahead)
                        registers = [r for r in members if r['node_type'] == 'reg_src']
                        self.assertEqual({r['node'] for r in registers},
                                         {'q0', 'q1', 'qa', 'qb', 'qr', 'ql'})
                        signatures.append(self.simulate(out, 'mt-state-tasks', env))
        self.assertEqual(len(set(signatures)), 1, signatures)

    def test_memory_commits_against_reference(self):
        signatures = []
        for lookahead in (0, 16):
            with self.subTest(lookahead=lookahead), tempfile.TemporaryDirectory(
                    prefix='mt-state-memory-') as directory:
                out = Path(directory)
                env, members = self.generate(out, 'mt-memory-commit', 4,
                                             lookahead=lookahead)
                self.assertTrue(any(r['node_type'] == 'memory' and r['role'] == 'member'
                                    for r in members))
                self.assertTrue(any(r['node_type'] == 'memory_writer' and
                                    r['role'] == 'pending_write_producer' for r in members))
                signatures.append(self.simulate(out, 'mt-memory-commit', env))
        self.assertEqual(len(set(signatures)), 1, signatures)

    def test_sparse_array_commits_and_resets_match_dense(self):
        for design in ('mt-sparse-register-reset', 'mt-sparse-register-async-reset'):
            signatures = []
            for max_writes in (0, 8):
                with self.subTest(design=design, max_writes=max_writes), tempfile.TemporaryDirectory(
                        prefix='mt-state-sparse-') as directory:
                    out = Path(directory)
                    env, members = self.generate(out, design, 2, extra=(
                        '--mt-sparse-register-min-bytes=1',
                        f'--mt-sparse-register-max-writes={max_writes}'))
                    self.assertEqual(sum(r['node_type'] == 'reg_src' and
                                         r['node'] == 'entries' for r in members), 1)
                    self.assertEqual(sum(r['node_type'] == 'sparse_writer' for r in members),
                                     0 if max_writes == 0 else 2)
                    signatures.append(self.simulate(out, design, env))
            self.assertEqual(len(set(signatures)), 1, (design, signatures))


if __name__ == '__main__':
    unittest.main()
