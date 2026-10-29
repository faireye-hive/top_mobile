#!/usr/bin/env python3
"""PORT-00: read-only coverage inventory for historical items, skills, recipes and Lua.

Examples:
  python3 sources/Tools/port_coverage_audit.py --audit-port
  python3 sources/Tools/port_coverage_audit.py --audit-port --csv-dir /tmp/port-audit

The audit never executes historical Lua or changes game data. D0-D4 values are
only assigned from the explicit override ledger; static source matches are
reported as evidence and never promoted to a completed gameplay flow.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OVERRIDES = Path(__file__).with_name("port_coverage_overrides.csv")
LEVELS = {"D0", "D1", "D2", "D3", "D4"}
FUNCTION_PATTERNS = (
    re.compile(r"\bfunction\s+([A-Za-z_][\w.:]*)\s*\("),
    re.compile(r"\blocal\s+function\s+([A-Za-z_][\w]*)\s*\("),
)


def read_rows(path: Path) -> tuple[list[list[str]], int]:
    # Historical tables use a legacy single-byte encoding; preserve every cell.
    raw = path.read_bytes().decode("latin1")
    result = []
    malformed = 0
    for line_no, line in enumerate(raw.splitlines(), 1):
        if not line.strip() or line.lstrip().startswith(("#", "/")):
            continue
        cells = line.split("\t")
        if cells[0].strip().isdigit():
            result.append(cells)
        else:
            malformed += 1
    return result, malformed


def read_overrides(path: Path) -> dict[tuple[str, str], dict[str, str]]:
    if not path.exists():
        return {}
    result = {}
    with path.open(encoding="utf-8-sig", newline="") as f:
        for row in csv.DictReader(f):
            kind, key, level = (row.get(k, "").strip() for k in ("entity_type", "entity_key", "level"))
            if not kind and not key and not level:
                continue
            if kind not in {"item", "item_action", "skill", "skill_signature", "lua_function", "npc_action"}:
                raise ValueError(f"tipo de override invalido: {kind!r}")
            if level not in LEVELS:
                raise ValueError(f"nivel de override invalido para {kind}:{key}: {level!r}")
            if level != "D0" and not row.get("evidence", "").strip():
                raise ValueError(f"override {kind}:{key} em {level} exige evidencia rastreavel")
            result[(kind, key)] = row
    return result


def source_evidence(engine_root: Path, values: set[str]) -> dict[str, list[str]]:
    found: dict[str, set[str]] = defaultdict(set)
    if not engine_root.exists():
        return {}
    for path in engine_root.rglob("*"):
        if path.suffix not in {".cpp", ".h", ".hpp"} or "tests" in path.parts:
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for value in values:
            if value and re.search(r"(?<!\w)" + re.escape(value) + r"(?!\w)", text):
                found[value].add(path.relative_to(engine_root.parent).as_posix())
    return {key: sorted(paths) for key, paths in found.items()}


def lua_inventory(root: Path) -> tuple[list[dict[str, str]], list[dict[str, str]], dict[str, int]]:
    files = sorted(root.rglob("*.lua")) if root.exists() else []
    hashes: dict[str, list[str]] = defaultdict(list)
    functions: dict[str, set[str]] = defaultdict(set)
    for path in files:
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        rel = path.relative_to(root).as_posix()
        hashes[digest].append(rel)
        text = data.decode("latin1")
        for pattern in FUNCTION_PATTERNS:
            for name in pattern.findall(text):
                functions[name].add(rel)
    fn_rows = [{"function": name, "files": ";".join(sorted(paths)),
                "file_count": str(len(paths)), "coverage_level": "D0",
                "approximation": "false", "evidence": ""}
               for name, paths in sorted(functions.items())]
    duplicate_rows = []
    for digest, paths in sorted(hashes.items()):
        if len(paths) > 1:
            duplicate_rows.append({"sha256": digest, "copy_count": str(len(paths)),
                                   "files": ";".join(sorted(paths))})
    stats = {"lua_files": len(files), "lua_distinct_contents": len(hashes),
             "lua_duplicate_files": len(files) - len(hashes),
             "lua_distinct_function_names": len(functions)}
    return fn_rows, duplicate_rows, stats


def write_csv(path: Path, rows: list[dict[str, str]], columns: list[str]) -> None:
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=columns, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def production_inventory(client_root: Path, item_ids: set[int]) -> tuple[list[dict[str, str]], dict[str, int]]:
    """Inventory literal manufacturing/crafting/cooking recipes and resource gaps."""
    path = client_root / "scripts" / "offline" / "calculate" / "variable.lua"
    if not path.exists():
        path = client_root / "scripts" / "offline" / "maps" / "script" / "calculate" / "variable.lua"
    text = path.read_bytes().decode("latin1") if path.exists() else ""
    entry_pattern = re.compile(
        r"\b(Manufacturing|Crafting|Cooking)\s*\[\s*(\d+)\s*\]\s*=\s*\{([^}]*)\}"
    )
    field_pattern = re.compile(r"\b(Active|ItemID|ItemLv|Material1|Material2|Material3|Rad|GoodItem)\s*=\s*(-?\d+)")
    records = []
    counts: Counter[str] = Counter()
    # Known legacy recipe/iteminfo ID drift. The names and levels of the
    # resolved Client records match the comments and ItemLv in these rows;
    # runtime labels these narrowly scoped mappings PORT_APPROX.
    output_aliases = {
        ("Crafting", "539", 2984): 6090,  # Bow of Evanescence, Lv55
        ("Crafting", "688", 3464): 6091,  # Bow of Enigma, Lv65
    }
    output_alias_evidence = {
        ("Crafting", "539", 2984): "Client/server Crafting[539] comment Bow of Evanescence, ItemLv 55; iteminfo 6090 has the same name and level.",
        ("Crafting", "688", 3464): "Client/server Crafting[688] comment Bow of Enigma, ItemLv 65; iteminfo 6091 has the same name and level.",
    }
    for kind, recipe_index, body in entry_pattern.findall(text):
        values = {name: int(value) for name, value in field_pattern.findall(body)}
        if not all(name in values for name in ("Active", "ItemID", "ItemLv", "Material1", "Material2", "Material3", "Rad", "GoodItem")):
            continue
        outputs = values["ItemID"]
        materials = [values[f"Material{i}"] for i in range(1, 4)]
        resolved_output = output_aliases.get((kind, recipe_index, outputs), outputs)
        aliased_output = resolved_output != outputs
        missing_output = resolved_output > 0 and resolved_output not in item_ids
        missing_materials = [str(value) for value in materials if value > 0 and value not in item_ids]
        active = values["Active"] == 1
        counts[f"{kind.lower()}_records"] += 1
        if active:
            counts[f"active_{kind.lower()}_recipes"] += 1
        if active and missing_output:
            counts["active_recipes_with_missing_output"] += 1
        if active and aliased_output:
            counts["active_recipes_with_approximate_output_alias"] += 1
        if active and missing_materials:
            counts["active_recipes_with_missing_materials"] += 1
        records.append({
            "kind": kind,
            "recipe_index": recipe_index,
            "active": str(values["Active"]),
            "output_item_id": str(outputs),
            "resolved_output_item_id": str(resolved_output),
            "item_level": str(values["ItemLv"]),
            "material_ids": ";".join(map(str, materials)),
            "weight": str(values["Rad"]),
            "good_item": str(values["GoodItem"]),
            # A recipe whose output or required input is absent from the
            # historical item catalog cannot complete its input -> output
            # flow in the offline client. Keep inactive legacy rows at D1,
            # but make active data-blocked rows visible as PORT-00 gaps.
            "coverage_level": "D0" if active and (missing_output or missing_materials) else "D1",
            "approximation": "true" if aliased_output else "false",
            "approximation_evidence": output_alias_evidence.get((kind, recipe_index, outputs), ""),
            "output_in_iteminfo": "no" if missing_output else "yes",
            "missing_material_ids": ";".join(missing_materials),
        })
    counts["production_recipe_records"] = len(records)
    return sorted(records, key=lambda row: (row["kind"], int(row["recipe_index"]))), dict(counts)


def garner_npc_action_inventory(client_root: Path, engine_root: Path,
                                overrides: dict[tuple[str, str], dict[str, str]]) -> tuple[list[dict[str, str]], dict[str, int]]:
    """List actions wired by Garner NPC scripts without treating name matches as support."""
    npc_table = client_root / "scripts" / "offline" / "garner" / "garnernpc.txt"
    if not npc_table.exists():
        npc_table = client_root / "scripts" / "offline" / "maps" / "garner" / "garnernpc.txt"
    script_root = client_root / "scripts" / "offline" / "script" / "MisScript"
    if not script_root.exists():
        script_root = client_root / "scripts" / "offline" / "maps" / "script" / "MisScript"
    if not npc_table.exists() or not script_root.exists():
        return [], {"garner_npc_action_rows": 0, "garner_npc_scripts": 0}

    assigned: dict[str, set[str]] = defaultdict(set)
    for cells in npc_table.read_bytes().decode("latin1").splitlines():
        if not cells.strip() or cells.lstrip().startswith(("#", "/")):
            continue
        columns = cells.split("\t")
        if len(columns) <= 11:
            continue
        # NpcSystem::parse consumes column 11 as the Lua script. The table's
        # human-readable header is offset from the fields in this legacy file.
        script = columns[11].strip()
        if script and script != "0":
            assigned[script].add(columns[1].strip())

    # Lua NPC actions can be attached either to TriggerAction(...), or directly
    # to Text(page, label, Action, ...). Count both forms; source refs are
    # reported separately and never promoted to D2/D3 because they may be
    # tables, previews, or unreachable branches.
    action_pattern = re.compile(r"\bTriggerAction\s*\(\s*\d+\s*,\s*([A-Za-z_][\w]*)")
    text_action_pattern = re.compile(
        r'\bText\s*\(\s*\d+\s*,\s*"(?:\\.|[^"\\])*"\s*,\s*([A-Za-z_][\w]*)')
    function_pattern = re.compile(r"\bfunction\s+([A-Za-z_][\w.]*)\s*\(")
    occurrences: Counter[tuple[str, str, str]] = Counter()
    for path in sorted(script_root.glob("NpcScript*.lua")):
        raw = path.read_bytes().decode("latin1")
        # Remove single-line comments before scanning so commented-out legacy
        # actions do not inflate actionable coverage.
        text = re.sub(r"--[^\n]*", "", raw)
        starts = list(function_pattern.finditer(text))
        for i, start in enumerate(starts):
            function = start.group(1)
            if function not in assigned:
                continue
            end = starts[i + 1].start() if i + 1 < len(starts) else len(text)
            body = text[start.start():end]
            for action in action_pattern.findall(body) + text_action_pattern.findall(body):
                occurrences[(path.stem, function, action)] += 1

    values = {action for _, _, action in occurrences}
    refs = source_evidence(engine_root, values)
    rows = []
    for (script_file, function, action), count in sorted(occurrences.items()):
        override = overrides.get(("npc_action", f"{script_file}::{function}::{action}"))
        rows.append({"historical_npcs": ";".join(sorted(assigned[function])),
                     "lua_script": script_file, "lua_function": function,
                     "direct_action": action, "occurrences": str(count),
                     "coverage_level": override.get("level", "D0") if override else "D0",
                     "static_engine_references": ";".join(refs.get(action, [])),
                     "note": (override.get("evidence", "") if override else
                              "D0=historical action inventoried; static references do not prove a playable flow")})
    return rows, {"garner_npc_action_rows": len(rows),
                  "garner_npc_scripts": len(assigned),
                  "garner_npc_distinct_actions": len(values)}


def run_audit(client_root: Path, out_dir: Path | None, overrides_path: Path) -> dict:
    tables = client_root / "scripts" / "table"
    item_rows, malformed_item_rows = read_rows(tables / "iteminfo.txt")
    skill_rows, malformed_skill_rows = read_rows(tables / "skillinfo.txt")
    item_ids = {int(row[0]) for row in item_rows}
    recipes, recipe_stats = production_inventory(client_root, item_ids)
    cpp_root = ROOT / "sources" / "Engine"
    overrides = read_overrides(overrides_path)
    garner_actions, garner_stats = garner_npc_action_inventory(client_root, cpp_root, overrides)
    lua_root = client_root / "scripts" / "offline"
    lua_functions, lua_duplicates, lua_stats = lua_inventory(lua_root)
    lua_function_names = {row["function"] for row in lua_functions}

    actions = sorted({r[86].strip() for r in item_rows if len(r) > 86 and
                      r[86].strip() not in {"", "0"} and not r[86].strip().isdigit()})
    action_refs = source_evidence(cpp_root, set(actions))
    item_records, item_action_records = [], []
    action_counts = Counter()
    for row in item_rows:
        item_id = str(int(row[0]))
        name = row[1].strip() if len(row) > 1 else ""
        action = row[86].strip() if len(row) > 86 else ""
        action = action if action not in {"", "0"} and not action.isdigit() else ""
        action_counts[action or "(sem callback)"] += 1
        override = overrides.get(("item", item_id)) or overrides.get(("item_action", action))
        item_records.append({"id": item_id, "name": name, "item_action": action,
                             "coverage_level": override.get("level", "D1") if override else "D1",
                             "approximation": override.get("approximation", "false") if override else "false",
                             "evidence": override.get("evidence", "") if override else "",
                             "static_cpp_references": ";".join(action_refs.get(action, [])) if action else "",
                             "note": "D1=data/cadastro; callback detectado nao comprova fluxo completo"})
    for action in actions:
        override = overrides.get(("item_action", action))
        item_action_records.append({"item_action": action, "item_count": str(action_counts[action]),
                                    "coverage_level": override.get("level", "D1") if override else "D1",
                                    "approximation": override.get("approximation", "false") if override else "false",
                                    "evidence": override.get("evidence", "") if override else "",
                                    "static_cpp_references": ";".join(action_refs.get(action, [])),
                                    "lua_function_name_match": "yes" if action in lua_function_names else "no"})

    skill_records = []
    signatures: dict[str, dict] = {}
    for row in skill_rows:
        padded = row + [""] * max(0, 18 - len(row))
        # Functional policy signature excludes id/name and visual/effect columns.
        raw_signature = "\t".join(padded[2:19])
        signature = hashlib.sha256(raw_signature.encode("latin1")).hexdigest()[:16]
        signatures.setdefault(signature, {"signature": signature, "member_ids": [], "sample_fields_2_18": raw_signature})
        signatures[signature]["member_ids"].append(str(int(row[0])))
        sid = str(int(row[0]))
        override = overrides.get(("skill", sid)) or overrides.get(("skill_signature", signature))
        skill_records.append({"id": sid, "name": row[1].strip() if len(row) > 1 else "",
                              "behavior_signature": signature,
                              "coverage_level": override.get("level", "D1") if override else "D1",
                              "approximation": override.get("approximation", "false") if override else "false",
                              "evidence": override.get("evidence", "") if override else "",
                              "note": "D1=registro carregado; nao comprova regra completa de skill"})
    signature_records = [{"behavior_signature": key, "skill_count": str(len(v["member_ids"])),
                          "skill_ids": ";".join(v["member_ids"]), "sample_fields_2_18": v["sample_fields_2_18"],
                          "coverage_level": (overrides.get(("skill_signature", key)) or {}).get("level", "D1"),
                          "approximation": (overrides.get(("skill_signature", key)) or {}).get("approximation", "false"),
                          "evidence": (overrides.get(("skill_signature", key)) or {}).get("evidence", "")}
                         for key, v in sorted(signatures.items())]

    for record in lua_functions:
        override = overrides.get(("lua_function", record["function"]))
        if override:
            record.update(coverage_level=override.get("level", "D0"),
                          approximation=override.get("approximation", "false"),
                          evidence=override.get("evidence", ""))

    summary = {
        "client_root": str(client_root), "item_records": len(item_rows), "distinct_item_actions": len(actions),
        "malformed_item_rows_skipped": malformed_item_rows,
        "skill_records": len(skill_rows), "distinct_skill_behavior_signatures": len(signatures),
        "malformed_skill_rows_skipped": malformed_skill_rows,
        "lua_functions_with_definition": len(lua_functions), "override_entries": len(overrides),
        "items_by_action": dict(sorted(action_counts.items())), **lua_stats,
        **recipe_stats, **garner_stats,
        "coverage_policy": "Itens, skills, funcoes Lua e acoes NPC de Garner usam D0-D4 revisados via ledger: sem override, tabela=D1, funcao Lua e acao NPC=D0. Receitas ativas com insumo/saida ainda ausente no iteminfo sao D0; dois aliases historicos de saida estao marcados PORT_APPROX; referencias estaticas nao provam paridade.",
    }
    if out_dir:
        out_dir.mkdir(parents=True, exist_ok=True)
        write_csv(out_dir / "items.csv", item_records,
                  ["id", "name", "item_action", "coverage_level", "approximation", "evidence", "static_cpp_references", "note"])
        write_csv(out_dir / "item_actions.csv", item_action_records,
                  ["item_action", "item_count", "coverage_level", "approximation", "evidence", "static_cpp_references", "lua_function_name_match"])
        write_csv(out_dir / "skills.csv", skill_records,
                  ["id", "name", "behavior_signature", "coverage_level", "approximation", "evidence", "note"])
        write_csv(out_dir / "skill_signatures.csv", signature_records,
                  ["behavior_signature", "skill_count", "skill_ids", "sample_fields_2_18", "coverage_level", "approximation", "evidence"])
        write_csv(out_dir / "lua_functions.csv", lua_functions,
                  ["function", "files", "file_count", "coverage_level", "approximation", "evidence"])
        write_csv(out_dir / "lua_duplicate_files.csv", lua_duplicates, ["sha256", "copy_count", "files"])
        write_csv(out_dir / "production_recipes.csv", recipes,
                  ["kind", "recipe_index", "active", "output_item_id", "resolved_output_item_id", "item_level", "material_ids",
                   "weight", "good_item", "coverage_level", "approximation", "approximation_evidence",
                   "output_in_iteminfo", "missing_material_ids"])
        write_csv(out_dir / "garner_npc_actions.csv", garner_actions,
                  ["historical_npcs", "lua_script", "lua_function", "direct_action", "occurrences",
                   "coverage_level", "static_engine_references", "note"])
        (out_dir / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return summary


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Auditoria PORT-00 read-only de itens, skills, receitas e callbacks Lua")
    parser.add_argument("--audit-port", action="store_true", help="executa a auditoria de cobertura")
    parser.add_argument("--client-root", type=Path, default=ROOT / "Client", help="raiz do cliente com scripts/table e scripts/offline")
    parser.add_argument("--csv-dir", type=Path, help="diretorio de saida dos CSVs e summary.json")
    parser.add_argument("--overrides", type=Path, default=OVERRIDES, help="ledger CSV com classificacoes D0-D4 revisadas")
    args = parser.parse_args(argv)
    if not args.audit_port:
        parser.error("informe --audit-port")
    try:
        summary = run_audit(args.client_root.resolve(), args.csv_dir.resolve() if args.csv_dir else None,
                            args.overrides.resolve())
    except (OSError, ValueError, csv.Error) as exc:
        print(f"port_coverage_audit: {exc}", file=sys.stderr)
        return 2
    printable = {key: value for key, value in summary.items() if key != "items_by_action"}
    print(json.dumps(printable, ensure_ascii=False, indent=2))
    if summary["malformed_item_rows_skipped"] or summary["malformed_skill_rows_skipped"]:
        print("Aviso: linhas sem ID numerico foram ignoradas; veja malformed_*_rows_skipped.")
    if args.csv_dir:
        print(f"Relatorios CSV/JSON: {args.csv_dir.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
