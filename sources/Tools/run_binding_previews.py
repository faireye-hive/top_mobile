#!/usr/bin/env python3
"""Run measured candidate previews with isolated user data, under Xvfb."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def plan(path):
    with Path(path).open(newline="") as stream:
        rows = csv.DictReader(stream)
        return sorted({(int(row["skill_id"]), "flight" if row["channel"] == "flight" else "hit" if row["channel"] == "target" else "cast")
                       for row in rows if row.get("non_billboard_binding_candidate") == "1" or
                       row["status"] in ("requires_live_dummy", "requires_live_dummy_and_opt_in", "potential_eff_part_truncation")})


def hashes(directory):
    return {str(p.relative_to(directory)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in directory.rglob("*") if p.is_file()} if directory.is_dir() else {}


def run(binary, client, candidates, output, character="1", time=.35, timeout=120):
    binary, client, candidates, output = map(lambda p: Path(p).resolve(), (binary, client, candidates, output))
    if client == output or client in output.parents or output == client.parent:
        raise ValueError("output must be a new directory outside Client")
    cases = plan(candidates)
    output.mkdir(parents=True, exist_ok=False)
    session = output / "session"
    isolated = session / "Client"
    isolated.mkdir(parents=True)
    before = hashes(client / "user")
    for source in client.iterdir():
        target = isolated / source.name
        if source.name == "user":
            shutil.copytree(source, target)
        else:
            target.symlink_to(source, target_is_directory=source.is_dir())
    (isolated / "user").mkdir(exist_ok=True)
    options = {"LIBGL_ALWAYS_SOFTWARE": "1", "SDL_AUDIODRIVER": "dummy"}
    # Rendering uses the normal engine policy. This option only records draws.
    options["TOP_HISTORICAL_BINDING_TRACE"] = "1"
    environment = {k: v for k, v in os.environ.items()
                   if not k.startswith("TOP_HISTORICAL_") and
                   k not in ("TOP_FROZEN_RESULT_GROUND", "TOP_STRIP_NESTED_LOOP", "TOP_EFF_TIME_MODE")}
    summary = {"scope": "preview", "behavior_policy": "normal_engine", "character": character, "initial_step_seconds": time, "environment": options,
               "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
               "candidates_sha256": hashlib.sha256(candidates.read_bytes()).hexdigest(), "runs": []}
    for skill, channel in cases:
        stem = str(skill) + "-" + channel
        shot = output / (stem + ".ppm")
        command = [str(binary), "--character", character, "--skill-preview", str(skill),
                   "--skill-preview-channel", channel, "--skill-preview-time", str(time), "--shot", str(shot)]
        with (output / (stem + ".log")).open("w") as log:
            try:
                result = subprocess.run(command, cwd=session, env=dict(environment, **options), stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
                code = result.returncode
            except subprocess.TimeoutExpired:
                code = "timeout"
        summary["runs"].append({"skill": skill, "channel": channel, "exit_code": code, "shot_exists": shot.is_file()})
        summary["original_user_unchanged"] = hashes(client / "user") == before
        (output / "runs.json").write_text(json.dumps(summary, indent=2) + "\n")
        print(stem, "exit", code, "shot", shot.is_file(), flush=True)
        if not summary["original_user_unchanged"]:
            raise RuntimeError("original user data changed; stopping preview audit")
    with (output / "combined.log").open("w") as combined:
        for skill, channel in cases:
            with (output / (str(skill) + "-" + channel + ".log")).open() as log:
                shutil.copyfileobj(log, combined)
    return summary


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=Path("build/top_gl"))
    parser.add_argument("--client", type=Path, default=Path("Client"))
    parser.add_argument("--candidates", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--character", choices=("1", "2", "3", "4"), default="1")
    parser.add_argument("--time", type=float, default=.35)
    parser.add_argument("--timeout", type=float, default=120)
    args = parser.parse_args()
    if not 0 <= args.time <= 10 or not args.timeout > 0:
        parser.error("--time must be 0..10; --timeout must be positive")
    result = run(args.binary, args.client, args.candidates, args.out, args.character, args.time, args.timeout)
    raise SystemExit(0 if all(r["exit_code"] == 0 and r["shot_exists"] for r in result["runs"]) else 1)
