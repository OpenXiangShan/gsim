#!/usr/bin/env python3

from pathlib import Path

llc_slice_size = 96*1024*1024/16 # Configured for Zen4-X3D

def extract_perf_result(folder: Path, suffix = ""):
    with open(folder / f"err{suffix}.log", "r") as file:
        lines = file.readlines()
        # extract perf stat
        perf_start_line = -1
        perf_stop_line = -1
        for idx in range(len(lines)):
            if lines[idx].startswith(" Performance counter stats"):
                perf_start_line = idx
            if lines[idx].strip().endswith(" seconds sys"):
                perf_stop_line = idx
        assert(perf_start_line != -1 and perf_stop_line != -1)
        perf_stat = lines[perf_start_line+2:perf_stop_line]
        stat = dict()
        for x in perf_stat:
            line = x.strip()
            if line.find("(") != -1:
                line = line[:line.index("(")].strip()
            if line.find("#") != -1:
                line = line[:line.index("#")].strip()
            if len(line) == 0:
                continue
            line = line.split()
            value = line[0].replace(',','')
            if '.' in value:
                number = float(value)
            else:
                number = int(value)
            key = " ".join(line[1:])
            stat[key] = number
        return stat

def extract_l3_result(folder: Path, suffix = ""):
    with open(folder / f"l3{suffix}.log", "r") as file:
        l3 = []
        for line in file.readlines():
            l3.append(int(line.strip()))
        return l3

def extract_result(folder: Path, suffix = ""):
    stat = extract_perf_result(folder, suffix)
    l3 = extract_l3_result(folder, suffix)
    stat['name'] = "-".join(folder.name.split("-")[:-1])
    stat['llc-size'] = int(folder.name.split("-")[-1]) * llc_slice_size
    stat['l3_0-90p'] = sorted(l3)[int(len(l3)*0.90)]
    return stat

def extract_result_to_csv(result_dir: Path):
    result = []
    for x in sorted(result_dir.iterdir()):
        if x.is_dir():
            stat = extract_result(x)
            result.append(stat)
    keys = ['name', 'llc-size'] + [key for key in sorted(result[0].keys()) if key != 'name' and key != 'llc-size']
    buf = ""
    buf += ",".join(keys) + "\n"
    for stat in result:
        buf += ",".join(str(stat[key]) for key in keys) + "\n"
    return buf

def extract_result_multicopy_to_csv(result_dir: Path):
    result = []
    data = dict()
    for x in sorted(result_dir.iterdir()):
        if x.is_dir():
            name, xs_copy, l3_0_cache_unit = x.name.split("-")
            xs_copy = int(xs_copy)
            total_cycles = 0
            for i in range(xs_copy):
                stat = extract_result(x, f"-{i}")
                total_cycles += stat['cycles']
            data_key = (name, xs_copy)
            if data_key not in data:
                data[data_key] = dict()
            data[data_key][int(l3_0_cache_unit)] = total_cycles / xs_copy
    res = dict()
    for key in data:
        best_cycles = min(data[key].values())
        best_l3_0_cache_unit = min(data[key], key=data[key].get)
        for l3_0_cache_unit in data[key]:
            best_l3_0_cache_unit = l3_0_cache_unit if data[key][l3_0_cache_unit] * 0.99 < best_cycles and l3_0_cache_unit < best_l3_0_cache_unit else best_l3_0_cache_unit
        res[key] = f"{best_l3_0_cache_unit * llc_slice_size / 1024 / 1024} MB"
    return "\n".join(f"{key[0]}-{key[1]}copy,{res[key]}" for key in res)

if __name__ == "__main__":
    with open(Path("result-multicopy").resolve() / "result-multicopy.csv", "w") as file:
        file.write(extract_result_multicopy_to_csv(Path("result-multicopy").resolve()))
    with open(Path("result").resolve() / "result.csv", "w") as file:
        file.write(extract_result_to_csv(Path("result").resolve()))
