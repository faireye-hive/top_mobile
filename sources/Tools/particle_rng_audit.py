#!/usr/bin/env python3
"""Read-only source inventory of global RNG seeds and consumers in DX9 and GL."""
import argparse
import csv
import json
import re
from collections import Counter
from pathlib import Path


CALL_RE = re.compile(r"(?<![\w.])(?:std\s*::\s*)?(srand|rand)\s*\(")
GL_HELPER_RE = re.compile(
    r"\b(dx9Randf|deterministicRandBits|deterministicRandModulo|rand01|randfMappingEnabled)\b"
)
EXTENSIONS = {".c", ".cc", ".cpp", ".h", ".hpp", ".inl"}


def cpp_code_only(text):
    """Blank comments and literals while preserving byte offsets and newlines."""
    out = list(text)
    i = 0
    state = "code"
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""
        if state == "code":
            if ch == "/" and nxt == "/":
                out[i] = out[i + 1] = " "
                i += 2
                state = "line"
                continue
            if ch == "/" and nxt == "*":
                out[i] = out[i + 1] = " "
                i += 2
                state = "block"
                continue
            if ch == '"':
                out[i] = " "
                state = "string"
            elif ch == "'":
                out[i] = " "
                state = "char"
        elif state == "line":
            if ch == "\n":
                state = "code"
            else:
                out[i] = " "
        elif state == "block":
            if ch == "*" and nxt == "/":
                out[i] = out[i + 1] = " "
                i += 2
                state = "code"
                continue
            if ch != "\n":
                out[i] = " "
        elif state in ("string", "char"):
            end = '"' if state == "string" else "'"
            if ch == "\\":
                out[i] = " "
                if i + 1 < len(text):
                    if text[i + 1] != "\n":
                        out[i + 1] = " "
                    i += 2
                    continue
            elif ch == end:
                out[i] = " "
                state = "code"
            elif ch != "\n":
                out[i] = " "
        i += 1
    return "".join(out)


def source_files(root, dirs):
    root = Path(root)
    for dirname in dirs:
        folder = root / dirname
        if not folder.is_dir():
            continue
        for path in sorted(folder.rglob("*")):
            if path.is_file() and path.suffix.lower() in EXTENSIONS:
                yield path


def collect_sites(original_root, gl_root):
    rows = []
    counts = Counter()
    original_root = Path(original_root)
    gl_root = Path(gl_root)
    for path in source_files(original_root, ("Client", "Engine", "Libraries")):
        text = path.read_bytes().decode("latin1")
        code = cpp_code_only(text)
        for match in CALL_RE.finditer(code):
            line = code.count("\n", 0, match.start()) + 1
            relative = path.relative_to(original_root).as_posix()
            source_line = text.splitlines()[line - 1].strip()
            scope = relative.split("/", 1)[0]
            site_class = classify_site(relative, source_line)
            rows.append(("DX9", scope, match.group(1), site_class, relative,
                         line, source_line))
            counts[("DX9", scope, match.group(1), site_class)] += 1
    for path in source_files(gl_root, ("include", "src")):
        text = path.read_bytes().decode("utf-8", errors="replace")
        code = cpp_code_only(text)
        rel = path.relative_to(gl_root).as_posix()
        for match in CALL_RE.finditer(code):
            line = code.count("\n", 0, match.start()) + 1
            source_line = text.splitlines()[line - 1].strip()
            rows.append(("OpenGL", "Engine", match.group(1), "runtime_candidate",
                         rel, line, source_line))
            counts[("OpenGL", "Engine", match.group(1), "runtime_candidate")] += 1
        for match in GL_HELPER_RE.finditer(code):
            line = code.count("\n", 0, match.start()) + 1
            source_line = text.splitlines()[line - 1].strip()
            rows.append(("OpenGL", "Engine", "deterministic_helper", "runtime_candidate",
                         rel, line, source_line))
            counts[("OpenGL", "Engine", "deterministic_helper", "runtime_candidate")] += 1
    rows.sort(key=lambda row: (row[0], row[4], row[5], row[2]))
    return rows, counts


def classify_site(relative, source_line):
    if relative.startswith("Libraries/MaxSDK/") and re.search(
            r"\b(?:CoreExport|UtilExport)\b.*\b(?:srand|rand)\s*\(", source_line):
        return "declaration"
    if relative.startswith("Libraries/Util/logvwr/"):
        return "tool_source"
    return "runtime_candidate"


def audit(original_root, gl_root, output):
    rows, counts = collect_sites(original_root, gl_root)
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    with (output / "particle_rng_sites.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["backend", "scope", "kind", "classification", "source", "line", "code"])
        writer.writerows(rows)
    summary = {
        "original_root": str(original_root),
        "gl_root": str(gl_root),
        "counts": {f"{backend}:{scope}:{kind}:{classification}": count
                   for (backend, scope, kind, classification), count
                   in sorted(counts.items())},
        "evidence_limit": "Lexical source sites only; does not prove runtime reachability or call order.",
    }
    (output / "particle_rng_summary.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original-root", default="arquivo/fontes")
    parser.add_argument("--gl-root", default="sources/Engine")
    parser.add_argument("--out", default="build/particle-rng-audit")
    args = parser.parse_args()
    summary = audit(args.original_root, args.gl_root, args.out)
    print("particle RNG source audit:", summary["counts"], "|", args.out)


if __name__ == "__main__":
    main()
