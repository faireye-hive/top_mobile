#!/usr/bin/env python3
"""Summarize opt-in binding traces; a draw attempt is not visual approval."""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path

PREFIX = "[fx-binding] "


def read_rows(path):
    lines = [line[len(PREFIX):] for line in Path(path).read_text(errors="replace").splitlines(True)
             if line.startswith(PREFIX)]
    if not lines:
        raise ValueError("no binding trace found; enable TOP_HISTORICAL_BINDING_TRACE=1")
    rows = list(csv.DictReader(io.StringIO("".join(lines))))
    # Concatenated independent runs may repeat the header.
    rows = [r for r in rows if r.get("skill_id") != "skill_id"]
    required = {"skill_id", "visual_id", "owner", "system", "part", "dummy_available",
                "bound_matrix", "bound_center", "vertices", "particle", "resource", "model"} | {"m" + str(i) for i in range(16)}
    if any(not required <= r.keys() or None in r or any(v is None for v in r.values()) for r in rows):
        raise ValueError("incomplete binding trace row")
    return rows


def audit(log, output, scope, candidates=None):
    import math
    rows = read_rows(log)
    groups = {}
    for row in rows:
        key = tuple(row[k] for k in ("skill_id", "visual_id", "owner", "system", "part", "particle", "resource", "model"))
        group = groups.setdefault(key, {"samples": 0, "nonempty_samples": 0, "dummy_samples": 0,
                                      "matrix_samples": 0, "center_samples": 0, "matrix_values": set()})
        group["samples"] += 1
        nonempty = int(row["vertices"]) > 0
        group["nonempty_samples"] += nonempty
        available = row["dummy_available"] == "1"
        group["dummy_samples"] += nonempty and available
        group["matrix_samples"] += nonempty and available and row["bound_matrix"] == "1"
        group["center_samples"] += nonempty and available and row["bound_center"] == "1"
        if available:
            matrix = tuple(float(row["m" + str(i)]) for i in range(16))
            if not all(math.isfinite(v) for v in matrix):
                raise ValueError("nonfinite dummy matrix in trace")
            group["matrix_values"].add(matrix)
    records = []
    keys = ["skill_id", "visual_id", "owner", "system", "part", "particle", "resource", "model"]
    for key, counts in sorted(groups.items()):
        record = dict(zip(keys, key))
        record.update({k: v for k, v in counts.items() if k != "matrix_values"})
        record["distinct_matrices"] = len(counts["matrix_values"])
        record["declared_scope"] = scope
        record["visual_validation"] = "not_performed"
        records.append(record)
    coverage = []
    if candidates:
        with Path(candidates).open(newline="") as f:
            for row in csv.DictReader(f):
                model = row.get("non_billboard_binding_candidate") == "1"
                center = row["status"] in ("requires_live_dummy", "requires_live_dummy_and_opt_in")
                if not (model or center):
                    continue
                owner = {"actor": "player_actor", "target": "target_actor",
                         "item": "player_item", "item_extra": "player_item"}[row["channel"]]
                observed = [r for r in records if (r["skill_id"], r["visual_id"], r["owner"], r["system"], r["part"], r["particle"], r["resource"]) ==
                            (row["skill_id"], row["effect_id"], owner, row["system"], row["part"], Path(row["par"]).name, Path(row["eff"]).name)]
                applied = sum(r["matrix_samples" if model else "center_samples"] for r in observed)
                dummy = sum(r["dummy_samples"] for r in observed)
                coverage.append({"skill_id": row["skill_id"], "channel": row["channel"], "effect_id": row["effect_id"],
                                 "system": row["system"], "part": row["part"], "eff": row["eff"],
                                 "rule": "model_matrix" if model else "billboard_center", "applied_samples": applied,
                                 "dummy_samples": dummy, "observation": "nonempty_rule_applied" if applied and dummy else
                                 "geometry_observed_without_rule" if any(r["nonempty_samples"] for r in observed) else "not_observed",
                                 "declared_scope": scope, "visual_validation": "not_performed"})
    output = Path(output)
    if any(p.name.lower() == "client" for p in (output.resolve(), *output.resolve().parents)):
        raise ValueError("audit output must be outside Client")
    output.mkdir(parents=True, exist_ok=True)
    def write(name, data, columns):
        with (output / name).open("w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=columns); writer.writeheader(); writer.writerows(data)
    write("binding_observations.csv", records, keys + ["samples", "nonempty_samples", "dummy_samples", "matrix_samples", "center_samples", "distinct_matrices", "declared_scope", "visual_validation"])
    write("binding_candidate_observations.csv", coverage, ["skill_id", "channel", "effect_id", "system", "part", "eff", "rule", "applied_samples", "dummy_samples", "observation", "declared_scope", "visual_validation"])
    summary = {"trace_rows": len(rows), "observed_parts": len(records), "candidate_rows": len(coverage),
               "candidate_rows_applied": sum(r["observation"] == "nonempty_rule_applied" for r in coverage),
               "candidate_skills_applied": sorted({int(r["skill_id"]) for r in coverage if r["observation"] == "nonempty_rule_applied"}),
               "declared_scope": scope, "evidence_limit": "CPU geometry emission; scope supplied by caller; not combat execution or visual parity proof"}
    summary["log_sha256"] = hashlib.sha256(Path(log).read_bytes()).hexdigest()
    if candidates:
        summary["candidates_sha256"] = hashlib.sha256(Path(candidates).read_bytes()).hexdigest()
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    return summary


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--scope", choices=("preview", "gameplay", "synthetic"), required=True)
    parser.add_argument("--candidates", type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(audit(args.log, args.out, args.scope, args.candidates), indent=2))
    except (ValueError, OSError, KeyError) as error:
        parser.exit(2, str(error) + "\n")
