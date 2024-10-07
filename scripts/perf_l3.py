#!/usr/bin/env python3

# Assume we have setup the resctrl group0 using scripts/init_resctrl.sh .

from pathlib import Path
import os
import asyncio
import signal
import psutil

def set_cache_size(l3_id: int, size: int):
    # size is how many unit
    size = min(16, size)
    os.system("echo \"L3:{:d}={:04x}\" > /sys/fs/resctrl/group0/schemata".format(l3_id, (1<<size) - 1))

async def stat_l3(pid, res):
    with open("/sys/fs/resctrl/group0/tasks", "a") as f:
        f.write(str(pid) + "\n")
    while True:
        with open("/sys/fs/resctrl/group0/mon_data/mon_L3_00/llc_occupancy", "r") as f:
            data = f.read()
            res.append(int(data))
        await asyncio.sleep(0.5)

async def run_emu(emu_bin: str, emu_args: [str], l3_log_path: Path, stdout_path: Path, stderr_path: Path):
    proc = await asyncio.create_subprocess_exec(
        "perf", "stat", "-e", "instructions,cycles,branch-instructions,branch-misses",
        "numactl", "--physcpubind=0-7,16-23", # configured for 7950X3D
        emu_bin, *emu_args,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE
    )
    print("Executing", " ".join(["perf", "stat", "-e", "instructions,cycles,branch-instructions,branch-misses",
        "numactl", "--physcpubind=0-7,16-23", # configured for 7950X3D
        emu_bin, *emu_args]))
    pid = proc.pid
    res = []
    stat = asyncio.create_task(stat_l3(pid, res))
    existing_stdout = bytearray()
    while True:
        line = await proc.stdout.readline()
        existing_stdout += line
        line_str = line.decode()
        if line == b"":
            break
        if "Exit with code" in line_str or "PASS" in line_str:
            break
    stat.cancel()
    try:
        for child in psutil.Process(pid).children(recursive=True):
            child.terminate()
    except:
        pass
    stdout, stderr = await proc.communicate()
    with open(stdout_path, "wb") as f:
        f.write(existing_stdout + stdout)
    with open(stderr_path, "wb") as f:
        f.write(stderr)
    with open(l3_log_path, "w") as f:
        f.write("\n".join(map(str, res)))
    
def run_test(prefix: str, emu: str, emu_args: [str], l3_0_unit: int):
    config_name = "{}-{}".format(prefix, l3_0_unit)
    result_path = Path("result").resolve() / config_name
    result_path.mkdir(parents=True, exist_ok=True)
    set_cache_size(0, l3_0_unit)
    asyncio.run(run_emu(emu, emu_args, result_path / 'l3.log', result_path / 'out.log', result_path / 'err.log'))

def build_emu(dutName: str, pgo: bool):
    os.system(f"make compile dutName={dutName} -j `nproc`")
    if pgo:
        os.system(f"make build-emu-pgo dutName={dutName} -j `nproc`")
    else:
        os.system(f"make build-emu dutName={dutName} -j `nproc`")

def run_matrix():
    emu_target = [
        # dutName,              emu_bin,                    Perf workload
        ("ysyx3",               "./build/emu/Snewtop",      "ready-to-run/bin/coremark-rocket.bin"),
        ("NutShell",            "./build/emu/SSimTop",      "ready-to-run/bin/coremark-NutShell.bin"),
        ("rocket",              "./build/emu/STestHarness", "ready-to-run/bin/coremark-rocket.bin"),
        ("boom",                "./build/emu/STestHarness", "ready-to-run/bin/coremark-rocket.bin"),
        ("small-boom",          "./build/emu/STestHarness", "ready-to-run/bin/coremark-rocket.bin"),
        ("xiangshan",           "./build/emu/SSimTop",      "ready-to-run/bin/coremark-NutShell.bin"),
        ("xiangshan-default",   "./build/emu/SSimTop",      "ready-to-run/bin/coremark-NutShell.bin")
    ]
    for target in emu_target:
        dutName, emu_bin, perf_workload = target
        build_emu(dutName, False)
        for l3_0_cache_unit in range(1,17):
            run_test(f"{dutName}", emu_bin, [perf_workload],l3_0_cache_unit)
        build_emu(dutName, True)
        for l3_0_cache_unit in range(1,17):
            run_test(f"{dutName}-pgo", emu_bin, [perf_workload], l3_0_cache_unit)

async def run_multiple_xs_inst():
    # build_emu("xiangshan-default",True)
    for xs_copy in range(2, 9):
        for l3_0_cache_unit in range(1, 17):
            result_path = Path("result-multicopy").resolve() / f"xs-{xs_copy}-{l3_0_cache_unit}"
            result_path.mkdir(parents=True, exist_ok=True)
            set_cache_size(0, l3_0_cache_unit)
            tasks = []
            for i in range(xs_copy):
                tasks.append(
                    asyncio.create_task(
                        run_emu("./build/emu/SSimTop",
                                ["ready-to-run/bin/coremark-NutShell.bin"],
                                Path(f"result-multicopy/xs-{xs_copy}-{l3_0_cache_unit}/l3-{i}.log"),
                                Path(f"result-multicopy/xs-{xs_copy}-{l3_0_cache_unit}/out-{i}.log"),
                                Path(f"result-multicopy/xs-{xs_copy}-{l3_0_cache_unit}/err-{i}.log"))))
            for task in tasks:
                await task

# run_matrix()
asyncio.run(run_multiple_xs_inst())
