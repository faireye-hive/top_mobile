#!/usr/bin/env python3
"""Validate author-written offline missions against the historical data tables."""
from __future__ import annotations

import argparse
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_FILE = ROOT / "sources/Tools/custom-missions.tsv"
DEFAULT_CLIENT = ROOT / "Client"


def table_ids(path: Path) -> set[int]:
    result: set[int] = set()
    with path.open("r", encoding="latin1") as source:
        for line in source:
            fields = line.rstrip("\r\n").split("\t", 1)
            if fields and fields[0].strip().isdigit():
                result.add(int(fields[0].strip()))
    return result


def bounded_int(value: str, low: int, high: int) -> int | None:
    try:
        parsed = int(value)
    except ValueError:
        return None
    return parsed if low <= parsed <= high else None


def validate(path: Path, client_root: Path) -> list[str]:
    item_ids = table_ids(client_root / "scripts/table/iteminfo.txt")
    monster_ids = table_ids(client_root / "scripts/table/characterinfo.txt")
    errors: list[str] = []
    seen: set[int] = set()
    with path.open("r", encoding="utf-8-sig", newline="") as source:
        for line_number, raw in enumerate(source, 1):
            line = raw.rstrip("\r\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            columns = line.split("\t")
            def issue(message: str) -> None:
                errors.append(f"{path}:{line_number}: {message}")

            if len(columns) != 8:
                issue(f"esperava 8 colunas TSV; encontrei {len(columns)}")
                continue
            mission_id = bounded_int(columns[0], 900000, 1000000)
            if mission_id is None:
                issue("ID deve ser um inteiro entre 900000 e 1000000")
            elif mission_id in seen:
                issue(f"ID de missao duplicado: {mission_id}")
            else:
                seen.add(mission_id)
            if not columns[1].strip():
                issue("titulo vazio")
            if not columns[2].strip():
                issue("texto/objetivo vazio")
            if bounded_int(columns[3], 1, 100) is None:
                issue("nivel minimo deve estar entre 1 e 100")
            if bounded_int(columns[5], 0, 1000000000) is None:
                issue("EXP deve estar entre 0 e 1000000000")
            if bounded_int(columns[6], 0, 1000000000) is None:
                issue("ouro deve estar entre 0 e 1000000000")

            total_trigger_flags = 0
            objective_specs = columns[4].split(";") if columns[4] else []
            if not objective_specs:
                issue("adicione ao menos um objetivo")
            for spec in objective_specs:
                parts = spec.split(":")
                if len(parts) != 3 or parts[0] not in {"kill", "item"}:
                    issue(f"objetivo invalido {spec!r}; use kill:ID:QTD ou item:ID:QTD")
                    continue
                entity_id = bounded_int(parts[1], 1, 1000000)
                count = bounded_int(parts[2], 1, 255)
                if entity_id is None:
                    issue(f"ID invalido no objetivo {spec!r}")
                elif parts[0] == "kill" and entity_id not in monster_ids:
                    issue(f"monstro {entity_id} nao existe em characterinfo.txt")
                elif parts[0] == "item" and entity_id not in item_ids:
                    issue(f"item {entity_id} nao existe em iteminfo.txt")
                if count is None:
                    issue(f"quantidade deve estar entre 1 e 255 no objetivo {spec!r}")
                else:
                    total_trigger_flags += count
            if total_trigger_flags > 255:
                issue("a soma das quantidades dos objetivos nao pode passar de 255")

            if columns[7]:
                for spec in columns[7].split(";"):
                    parts = spec.split(":")
                    if len(parts) != 2:
                        issue(f"recompensa invalida {spec!r}; use ID_ITEM:QTD")
                        continue
                    item_id = bounded_int(parts[0], 1, 1000000)
                    count = bounded_int(parts[1], 1, 1000000)
                    if item_id is None:
                        issue(f"ID invalido na recompensa {spec!r}")
                    elif item_id not in item_ids:
                        issue(f"item de recompensa {item_id} nao existe em iteminfo.txt")
                    if count is None:
                        issue(f"quantidade deve estar entre 1 e 1000000 na recompensa {spec!r}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Valida o TSV de missoes offline proprias sem alterar o arquivo.")
    parser.add_argument("--file", type=Path, default=DEFAULT_FILE, help="arquivo TSV de missoes")
    parser.add_argument("--client-root", type=Path, default=DEFAULT_CLIENT, help="pasta Client com scripts/table")
    args = parser.parse_args()
    try:
        errors = validate(args.file, args.client_root)
    except OSError as error:
        print(f"Erro ao ler dados: {error}", file=sys.stderr)
        return 2
    if errors:
        print("Foram encontrados problemas nas missoes:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    print(f"OK: arquivo validado; nenhuma missao foi adicionada ou modificada ({args.file}).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
