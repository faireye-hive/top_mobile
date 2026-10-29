#!/usr/bin/env python3
"""Read-only sceneffectinfo TXT/BIN audit derived from the supplied DX9 source.

Requires python3-cryptography. Win32 layout is intentional: serialized pointers
occupy four bytes, never the Python host's pointer size. No decrypted table is
written back to Client and no crypto constants are copied into this tool.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import struct


def clean_cpp(text):
    return re.sub(r"//[^\n]*", "", re.sub(r"/\*.*?\*/", "", text, flags=re.S))


def class_body(text, name):
    match = re.search(r"\bclass\s+" + re.escape(name) + r"\b[^;{]*\{", text)
    if not match:
        raise ValueError("source class missing: " + name)
    depth = 1
    for index in range(match.end(), len(text)):
        depth += (text[index] == "{") - (text[index] == "}")
        if depth == 0:
            return text[match.end():index]
    raise ValueError("unterminated source class: " + name)


def source_layout(raw_header, effect_header):
    """Parse member declarations, ignoring constructor bodies; use Win32 ABI."""
    fields, offset = {}, 0
    widths = {"BOOL": (4, "i"), "int": (4, "i"), "DWORD": (4, "I"),
              "float": (4, "f"), "char": (1, "s"), "void*": (4, "I")}
    for text, name in [(raw_header, "CRawDataInfo"), (effect_header, "CMagicInfo")]:
        depth = 0
        for line in class_body(clean_cpp(text), name).splitlines():
            if depth == 0 and line.strip().endswith(";"):
                match = re.fullmatch(r"\s*(BOOL|int|DWORD|float|char|void\s*\*)\s*(\w+)"
                                     r"(?:\[(\d+)\])?\s*(?:\{[^}]*\})?\s*;\s*", line)
                if not match:
                    raise ValueError("unsupported source member: " + line.strip())
                typ, field, count = match.groups()
                width, fmt = widths[typ.replace(" ", "")]
                count = int(count or 1)
                offset = (offset + width - 1) // width * width
                fields[field] = (offset, fmt, count)
                offset += width * count
            depth += line.count("{") - line.count("}")
    return fields, (offset + 3) // 4 * 4


def source_crypto(cpp):
    cpp = clean_cpp(cpp)
    if not re.search(r"SetKeyWithIV\(cluTableKey,\s*16,\s*cluTableIV,\s*16\)", cpp):
        raise ValueError("DX9 key/IV invocation changed")
    if not re.search(r"AuthenticatedDecryptionFilter[^;]+DEFAULT_FLAGS\s*,\s*12\s*\)", cpp):
        raise ValueError("DX9 GCM tag contract changed")
    constants = []
    for name in ("cluTableKey", "cluTableIV"):
        match = re.search(r"\b" + name + r"\[\]\s*=\s*\{([^}]+)\}", cpp)
        if not match:
            raise ValueError("DX9 crypto constant missing: " + name)
        value = bytes(int(x, 16) for x in re.findall(r"0x[0-9a-fA-F]+", match[1]))
        if len(value) != 16:
            raise ValueError("unsupported DX9 crypto constant size")
        constants.append(value)
    if "GCM<CryptoPP::AES>::Decryption" not in cpp or "Base64Decoder" in cpp:
        raise ValueError("unsupported active DX9 table cipher path")
    return tuple(constants)


def semantic_columns(effect_header):
    body = class_body(clean_cpp(effect_header), "CMagicSet")
    columns = {"szDataName": (1, "text")}
    for field, function, index in re.findall(
            r"pInfo->(\w+)\s*=\s*(Str2Int|Str2Float)\(ParamList\[(\d+)\]", body):
        columns[field] = (int(index) + 2, "float" if function == "Str2Float" else "int")
    for field, index in re.findall(r"strncpy\(pInfo->(\w+)\s*,\s*ParamList\[(\d+)\]", body):
        columns[field] = (int(index) + 2, "text")
    match = re.search(r"nDummyNum\s*=\s*Util_ResolveTextLine\(ParamList\[(\d+)\]", body)
    if not match or not re.search(r"pInfo->nDummy\[n\]\s*=\s*Str2Int\(lstr1\[n\]\.c_str\(\)\)", body):
        raise ValueError("DX9 dummy list reader changed")
    columns["nDummy"] = (int(match[1]) + 2, "dummy")
    return columns


def check_dummy_contract(effect_header, fields):
    cpp = clean_cpp(effect_header)
    checks = [r"memset\(nDummy,\s*-1,\s*sizeof\(nDummy\)\)",
              r"if\s*\(pInfo->nDummyNum\s*==\s*1\)\s*\{\s*if\s*\(pInfo->nDummy\[n\]\s*<\s*-1\)\s*\{\s*pInfo->nDummyNum\s*=\s*0",
              r"Util_ResolveTextLine\(ParamList\[\d+\]\.c_str\(\),\s*lstr1,\s*" + str(fields["nDummy"][2]) + r",\s*','\)"]
    if not all(re.search(pattern, cpp) for pattern in checks):
        raise ValueError("DX9 dummy defaults/count contract changed")


def dx9_split(value, separator):
    # Util_ResolveTextLine/Util_ResolveTextLine1 (util2.cpp:297-348):
    # retain the first empty token, skip repeated separators, no trailing token.
    if len(value) <= 1:
        return [value] if value else []
    values = re.split(re.escape(separator) + "+", value)
    return values[:-1] if values[-1] == "" else values


def dx9_int(value):
    match = re.match(r"\s*([+-]?\d+)", value)
    return int(match[1]) if match else 0


def dx9_float(value):
    match = re.match(r"\s*([+-]?(?:(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?|inf(?:inity)?|nan))", value, re.I)
    value = float(match[1]) if match else 0.0
    return struct.unpack("<f", struct.pack("<f", value))[0]


def txt_rows(path, fields, columns):
    rows = {}
    for line in path.read_text(encoding="latin1").splitlines():
        values = dx9_split(line.split("//", 1)[0], "\t")
        if len(values) < 2 or not values[0].strip().isdigit():
            continue
        record = {}
        for field, (column, kind) in columns.items():
            value = values[column] if column < len(values) else ""
            if kind == "text":
                record[field] = value[:fields[field][2]]
            elif kind == "float":
                record[field] = dx9_float(value)
            elif kind == "dummy":
                limit = fields[field][2]
                entries = [dx9_int(v) for v in dx9_split(value, ",")][:limit]
                record["nDummyNum"] = 0 if len(entries) == 1 and entries[0] < -1 else len(entries)
                record[field] = entries + [-1] * (limit - len(entries))
            else:
                record[field] = dx9_int(value)
        rows[int(values[0])] = record
    return rows


def binary_rows(data, crypto, fields, size):
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    if len(data) < 12 or len(data) > 64 * 1024 * 1024:
        raise ValueError("invalid encrypted table size")
    key, iv = crypto
    decryptor = Cipher(algorithms.AES(key), modes.GCM(iv, data[-12:], min_tag_length=12)).decryptor()
    plain = decryptor.update(data[:-12]) + decryptor.finalize()  # authentication mandatory
    if not plain or len(plain) % size:
        raise ValueError("authenticated payload does not match source Win32 layout")
    rows = {}
    for slot in range(len(plain) // size):
        record = {}
        for name, (offset, fmt, count) in fields.items():
            start = slot * size + offset
            if fmt == "s":
                value = plain[start:start + count].split(b"\0", 1)[0].decode("latin1")
            else:
                value = list(struct.unpack_from("<" + fmt * count, plain, start))
                if count == 1:
                    value = value[0]
            record[name] = value
        if record["bExist"] != 1:
            continue
        if record["nID"] != slot or record["nIndex"] != slot:
            raise ValueError("source layout/index mismatch at active slot " + str(slot))
        rows[slot] = record
    return rows, len(plain) // size


def audit(client, original, output, binding=None, server_table=None):
    client, original, output = map(Path, (client, original, output))
    if output.resolve() == client.resolve() or client.resolve() in output.resolve().parents:
        raise ValueError("audit output must be outside Client")
    raw_path = original / "Libraries/common/include/TableData.h"
    cpp_path = original / "Libraries/common/src/TableData.cpp"
    effect_path = original / "Client/src/EffectSet.h"
    util_path = original / "Libraries/Util/include/util2.h"
    raw, cpp, effect = [p.read_text(encoding="latin1") for p in (raw_path, cpp_path, effect_path)]
    fields, size = source_layout(raw, effect)
    columns = semantic_columns(effect)
    check_dummy_contract(effect, fields)
    base_fields, _ = source_layout(raw, "class CMagicInfo {\n};")
    expected = (set(fields) - set(base_fields) - {"nPhotoTexID"}) | {"szDataName"}
    if expected != set(columns) | {"nDummyNum"}:
        raise ValueError("not all source scene-table fields have audited TXT semantics")
    util = clean_cpp(util_path.read_text(encoding="latin1"))
    if not re.search(r"Str2Int\([^)]*\)\s*\{\s*return\s+atoi\(", util) or not re.search(r"Str2Float\([^)]*\)\s*\{\s*return\s+float\(atof\(", util):
        raise ValueError("DX9 numeric text conversion changed")
    if not re.search(r"ParamList.push_back\(pstrList\[i\s*\+\s*2\]\)", clean_cpp(raw)):
        raise ValueError("DX9 TXT column offset changed")
    binary_path = client / "scripts/table/sceneffectinfo.bin"
    txt_path = client / "scripts/table/sceneffectinfo.txt"
    binary, slots = binary_rows(binary_path.read_bytes(), source_crypto(cpp), fields, size)
    text = txt_rows(txt_path, fields, columns)
    server = txt_rows(Path(server_table), fields, columns) if server_table else {}
    folded = {}
    for path in (client / "effect").iterdir():
        if path.is_file():
            folded.setdefault(path.name.lower(), []).append(path.name)
    def lookup(record):
        if not record:
            return "no_scene_record"
        name = record["szDataName"]
        return "exact" if (client / "effect" / name).is_file() else "case_insensitive" if name.lower() in folded else "missing_literal"
    all_fields = set(columns) | {"nDummyNum"}
    differences, records = [], []
    for id in sorted(text.keys() | binary.keys()):
        t, b = text.get(id), binary.get(id)
        changed = [f for f in sorted(all_fields) if t is None or b is None or t[f] != b[f]]
        for field in changed:
            differences.append({"id": id, "field": field, "txt": repr(t[field]) if t else "missing",
                                "bin": repr(b[field]) if b else "missing"})
        name = (b or t)["szDataName"]
        records.append({"id": id, "txt_name": t["szDataName"] if t else "", "bin_name": b["szDataName"] if b else "",
                        "different_fields": ";".join(changed), "asset_lookup": lookup(b), "txt_asset_lookup": lookup(t),
                        "casefold_matches": ";".join(folded.get(name.lower(), []))})
    channels = []
    if binding:
        with Path(binding).open(encoding="utf-8", errors="surrogateescape", newline="") as f:
            for row in csv.DictReader(f):
                if row["status"] not in ("missing_par", "no_scene_par"):
                    continue
                id = int(row["effect_id"])
                b, t = binary.get(id), text.get(id)
                txt_lookup, bin_lookup = lookup(t), lookup(b)
                classification = "mode_or_asset_resolution_differs"
                if txt_lookup == bin_lookup == "no_scene_record":
                    classification = "absent_in_client_txt_and_bin"
                elif txt_lookup == bin_lookup == "missing_literal":
                    classification = "missing_literal_asset_in_both_modes"
                channels.append({"skill_id": row["skill_id"], "channel": row["channel"], "effect_id": id,
                                 "txt_name": t["szDataName"] if t else "", "bin_name": b["szDataName"] if b else "",
                                 "server_name": server[id]["szDataName"] if id in server else "",
                                 "txt_lookup": txt_lookup, "bin_lookup": bin_lookup, "classification": classification,
                                 "runtime_use_proven": "no"})
    summary = {"authenticated": True, "record_bytes": size, "slots": slots, "txt_records": len(text),
               "bin_records": len(binary), "different_ids": len({r['id'] for r in differences}),
               "different_fields": len(differences), "missing_literal_assets": sum(r["asset_lookup"] == "missing_literal" for r in records),
               "case_only_assets": sum(r["asset_lookup"] == "case_insensitive" for r in records), "unresolved_channels_examined": len(channels),
               "binary_sha256": hashlib.sha256(binary_path.read_bytes()).hexdigest(),
               "txt_sha256": hashlib.sha256(txt_path.read_bytes()).hexdigest(),
               "source_sha256": {str(p.relative_to(original)): hashlib.sha256(p.read_bytes()).hexdigest() for p in (raw_path, cpp_path, effect_path, util_path)}}
    output.mkdir(parents=True, exist_ok=True)  # only after authentication/layout checks
    def write(name, keys, rows):
        with (output / name).open("w", encoding="utf-8", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=keys); writer.writeheader(); writer.writerows(rows)
    write("scene_records.csv", ["id", "txt_name", "bin_name", "different_fields", "asset_lookup", "txt_asset_lookup", "casefold_matches"], records)
    write("scene_differences.csv", ["id", "field", "txt", "bin"], differences)
    write("unresolved_channels.csv", ["skill_id", "channel", "effect_id", "txt_name", "bin_name", "server_name", "txt_lookup", "bin_lookup", "classification", "runtime_use_proven"], channels)
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    return summary


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, default=Path("Client"))
    parser.add_argument("--original", type=Path, default=Path("arquivo/fontes"))
    parser.add_argument("--out", type=Path, default=Path("build/scene-table-audit"))
    parser.add_argument("--binding-channels", type=Path)
    parser.add_argument("--server-table", type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(audit(args.client, args.original, args.out, args.binding_channels, args.server_table), indent=2))
    except Exception as error:
        parser.exit(2, type(error).__name__ + ": " + str(error) + "\n")
