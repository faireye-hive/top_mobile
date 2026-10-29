#!/usr/bin/env python3
"""Publish only game resources; local saves, archives and executables stay private."""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import shutil
import gzip

RESOURCE_DIRS = {"animation", "model", "effect", "texture", "map", "scripts", "music", "cursor"}
EXTENSIONS = {
    ".lab", ".lgo", ".lmo", ".eff", ".par", ".map", ".rbo", ".obj",
    ".png", ".dds", ".bmp", ".tga", ".jpg", ".ani", ".ogg", ".wav",
    ".lua", ".txt", ".bin", ".blk", ".atr", ".clu", ".tx", ".csv",
    ".wsd", ".pk",
}


def package(client: Path, output: Path, compact=False, maps=None, pack_bytes=512 * 1024):
    client, output = client.resolve(), output.resolve()
    if not client.is_dir():
        raise ValueError(f"Client nao encontrado: {client}")
    if output == client or client in output.parents:
        raise ValueError("A saida deve ficar fora de Client")
    if not (client / "map" / "garner.map").is_file():
        raise ValueError("O mapa inicial Client/map/garner.map esta ausente")
    if compact:
        return package_compact(client, output, maps, pack_bytes)
    index = []
    total = 0
    public = output / "assets" / "Client"
    for directory in sorted(RESOURCE_DIRS):
        for source in sorted((client / directory).rglob("*")):
            if source.is_symlink() or not source.is_file() or source.suffix.lower() not in EXTENSIONS:
                continue
            # Do not follow a directory symlink out of the public resource tree.
            relative = source.relative_to(client)
            if any(part.startswith(".") for part in relative.parts) or any(
                parent.is_symlink() for parent in source.parents if parent != client and client in parent.parents
            ):
                continue
            destination = public / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            if destination.is_symlink():
                raise ValueError(f"Link inesperado na saida: {destination}")
            # Copy, never hardlink: later edits to Client must not mutate a release.
            shutil.copyfile(source, destination)
            with destination.open("rb") as stream:
                digest = hashlib.file_digest(stream, "sha256").digest()
            size = destination.stat().st_size
            index.append({"path": "Client/" + relative.as_posix(), "size": size,
                          "sha256": base64.b64encode(digest).decode("ascii")})
            total += size
    # Remove only obsolete files inside the tool-owned public resource folders.
    expected = {entry["path"][len("Client/"):] for entry in index}
    for directory in RESOURCE_DIRS:
        for path in (public / directory).rglob("*"):
            if path.is_file() and path.relative_to(public).as_posix() not in expected:
                path.unlink()
    output.mkdir(parents=True, exist_ok=True)
    temporary = output / "assets-index.json.tmp"
    temporary.write_text(json.dumps({"version": 1, "files": index}, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
    temporary.replace(output / "assets-index.json")
    print(f"{len(index)} recursos publicados ({total / 1024 / 1024:.1f} MiB). Download sob demanda; Client/user excluido.")


def public_resources(client, maps=None):
    """Use an allowlist, including each ancestor, before reading any bytes."""
    maps = {name.casefold() for name in maps} if maps else None
    if maps is not None:
        maps.add("garner")  # The shared application uses its base tables/root.
    for directory in sorted(RESOURCE_DIRS):
        for source in sorted((client / directory).rglob("*")):
            relative = source.relative_to(client)
            if source.is_symlink() or not source.is_file() or source.suffix.lower() not in EXTENSIONS:
                continue
            if any(part.startswith(".") for part in relative.parts) or any(
                parent.is_symlink() for parent in source.parents if parent != client and client in parent.parents
            ):
                continue
            if maps and directory == "map" and source.stem.casefold() not in maps:
                continue
            if maps and relative.parts[:3] == ("scripts", "offline", "maps") and len(relative.parts) > 3:
                if relative.parts[3].casefold() not in maps:
                    continue
            yield relative, source


def package_compact(client, output, maps=None, pack_bytes=512 * 1024):
    if pack_bytes < 1024:
        raise ValueError("Blocos devem ter pelo menos 1 KiB")
    public = output / "packs"
    public.mkdir(parents=True, exist_ok=True)
    files, packs, pending = [], [], []
    data = bytearray()
    previous_group = None

    def flush():
        if not pending:
            return
        encoded = gzip.compress(bytes(data), compresslevel=6, mtime=0)
        digest = hashlib.sha256(encoded).digest()
        name = digest.hex() + ".pack"
        destination = public / name
        if not destination.exists():
            temporary = destination.with_suffix(".tmp")
            temporary.write_bytes(encoded)
            temporary.replace(destination)
        number = len(packs)
        packs.append({"url": "packs/" + name, "codec": "gzip", "size": len(encoded),
                      "unpackedSize": len(data), "sha256": base64.b64encode(digest).decode("ascii")})
        files.extend(dict(entry, pack=number) for entry in pending)
        data.clear()
        pending.clear()

    for relative, source in public_resources(client, maps):
        size = source.stat().st_size
        group = relative.parent.as_posix()
        if group != previous_group or (data and len(data) + size > pack_bytes):
            flush()
        previous_group = group
        raw = source.read_bytes()
        pending.append({"path": "Client/" + relative.as_posix(), "size": len(raw), "offset": len(data),
                        "sha256": base64.b64encode(hashlib.sha256(raw).digest()).decode("ascii")})
        data.extend(raw)
    flush()
    index = {"version": 2, "files": files, "packs": packs}
    temporary = output / "assets-index.json.tmp"
    temporary.write_text(json.dumps(index, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
    temporary.replace(output / "assets-index.json")
    (output / "assets-index.json.gz").write_bytes(gzip.compress((output / "assets-index.json").read_bytes(), mtime=0))
    # Content-addressed packs may be used by an already-open session. Pruning
    # is explicit, so preparing an update cannot break that session's downloads.
    raw_bytes = sum(f["size"] for f in files)
    unique_packs = {p["url"]: p["size"] for p in packs}
    packed_bytes = sum(unique_packs.values())
    report = {"files": len(files), "packs": len(packs), "originalBytes": raw_bytes,
              "uniquePacks": len(unique_packs), "packedBytes": packed_bytes,
              "maps": sorted(maps) if maps else "all"}
    (output / "package-report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    (output / ".nojekyll").touch()
    print(f"{len(files)} recursos em {len(packs)} blocos: {raw_bytes / 1048576:.1f} -> "
          f"{packed_bytes / 1048576:.1f} MiB. Saves privados excluidos.")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, default=Path("Client"))
    parser.add_argument("--output", type=Path, default=Path("build-web"))
    parser.add_argument("--compact", action="store_true", help="Blocos gzip para hospedagem estatica")
    parser.add_argument("--map", action="append", dest="maps", help="Mapa a publicar; pode repetir (padrao: todos)")
    args = parser.parse_args()
    package(args.client, args.output, compact=args.compact, maps=args.maps)
