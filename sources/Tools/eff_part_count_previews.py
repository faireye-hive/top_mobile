#!/usr/bin/env python3
"""Derive preview candidates from EFF count audit and attributed dependencies."""
import argparse
import csv
from pathlib import Path


def build_plan(counts, dependencies, output):
    with Path(counts).open(newline="") as stream:
        assets = list(csv.DictReader(stream))
    with Path(dependencies).open(newline="") as stream:
        deps = list(csv.DictReader(stream))
    rows = set()
    for asset in assets:
        nested = asset["potential_nested_truncation"] == "1"
        direct = any(asset[k] == "1" for k in ("potential_billboard_truncation", "potential_native_truncation", "potential_external_truncation"))
        if not (nested or direct):
            continue
        parents = {Path(p).name for p in asset["direct_par_files"].split(";") if p}
        for dep in deps:
            resource = Path(dep["resolved_path_or_status"]).name
            if not ((nested and dep["dependency_type"] in ("scene_par", "par") and resource in parents) or
                    (direct and dep["dependency_type"] == "eff" and resource == Path(asset["file"]).name)):
                continue
            if not dep["skill_id"].isdigit():
                continue
            channel = {"actor": "actor", "item": "item", "item_extra": "item_extra", "flight": "flight",
                       "impact_target": "target", "impact_ground": "target", "impact_water": "target"}.get(dep["channel"])
            if channel:
                rows.add((dep["skill_id"], channel, "potential_eff_part_truncation", asset["file"], resource))
    output = Path(output)
    if any(p.name.lower() == "client" for p in (output.resolve(), *output.resolve().parents)):
        raise ValueError("preview plan output must be outside Client")
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="") as stream:
        writer = csv.writer(stream); writer.writerow(["skill_id", "channel", "status", "eff", "dependency"])
        writer.writerows(sorted(rows))
    return len(rows)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--counts", type=Path, required=True)
    parser.add_argument("--deps", type=Path, default=Path("sources/auditoria-historica/dependencies.csv"))
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    print("Preview candidate rows:", build_plan(args.counts, args.deps, args.out))
