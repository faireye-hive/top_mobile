import csv
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

AUDIT = sys.argv[1] if len(sys.argv) > 1 else None
PROJECT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT / "sources"))
from Tools.eff_part_count_previews import build_plan
from Tools.run_binding_previews import plan


def name(value):
    return value.encode().ljust(32, b"\0")


def eff(count, model, billboard):
    header = (struct.pack("<IiB", 6, 0, 0) + name("") + struct.pack("B", 0) + name("") +
              struct.pack("<B4fi", 0, 0., 0., 0., 0., count))
    part = (name("fixture") + struct.pack("<iiifHff", 1, 5, 7, 1., 2, 0., 1.) +
            struct.pack("<6f", *([1.] * 6)) + struct.pack("<12f", *([0.] * 12)) +
            struct.pack("<8f", *([1.] * 8)) + struct.pack("<HHfHf", 0, 0, 0., 0, 0.) +
            name("bound.tga") + name(model) +
            struct.pack("<Bii3fHffiB4fB", billboard, 1, 8, 3., 1., 2., 0, 0., 0., 0, 0, 0., 0., 0., 0., 0))
    return header + part * count


def par(model):
    system = (struct.pack("<i", 6) + name("parent") + struct.pack("<i", 1) + name("bound.tga") + name(model) +
              struct.pack("<3fHf3f4fB4i9fB", 0., 0., 0., 1, 1., 0., 0., 0., 1., 1., 1., 1., 0,
                          5, 7, 1, 1, 4., 0., 0., 0., 0., 0., 0., 0., 0., 0) + name("") +
              struct.pack("<5fBB", 0., 0., 0., 0., 2., 0, 0) + name("") + struct.pack("<iB", 2, 0))
    return struct.pack("<I", 14) + name("controller") + struct.pack("<if", 1, 4.) + system


@unittest.skipUnless(AUDIT, "requires compiled coverage audit")
class EffPartCountAuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.client = self.root / "Client"
        (self.client / "effect").mkdir(parents=True)
        for filename, data in {"board.eff": eff(9, "RectPlane", 1),
                               "native.eff": eff(17, "RectPlane", 0),
                               "external.eff": eff(9, "mesh.lgo", 0),
                               "overbound.eff": eff(65, "RectPlane", 0),
                               "a.par": par("board.eff")}.items():
            (self.client / "effect" / filename).write_bytes(data)
        self.deps = self.root / "deps.csv"
        self.deps.write_text((PROJECT / "sources/auditoria-historica/dependencies.csv").read_text().splitlines()[0] + "\n")

    def audit(self, source):
        output = self.root / "out"
        result = subprocess.run([AUDIT, "--client", str(self.client), "--out", str(output),
                                 "--source", str(source), "--deps", str(self.deps),
                                 "--table", str(PROJECT / "sources/auditoria-historica/fx_port_coverage.csv")],
                                cwd=PROJECT, capture_output=True, text=True, env=os.environ)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        with (output / "fx_eff_part_counts.csv").open() as f:
            rows = {r["file"]: r for r in csv.DictReader(f)}
        return output, rows

    def test_all_counts_and_limits_come_from_current_renderer(self):
        output, rows = self.audit(PROJECT / "sources/Engine/src/top/EffectSystem.cpp")
        self.assertEqual(set(rows), {"effect/board.eff", "effect/native.eff", "effect/external.eff"})
        self.assertEqual(rows["effect/board.eff"]["potential_nested_truncation"], "0")
        self.assertEqual(rows["effect/native.eff"]["potential_native_truncation"], "0")
        self.assertEqual(rows["effect/external.eff"]["potential_external_truncation"], "0")
        self.assertEqual(rows["effect/board.eff"]["billboard_compat_limit"], "none")
        self.assertTrue(all(r["runtime_clipping_proven"] == "no" for r in rows.values()))
        with (output / "fx_decode_failures.csv").open() as f:
            self.assertTrue(any(r["file"] == "effect/overbound.eff" for r in csv.DictReader(f)))

    def renderer_with_limit(self, value):
        original = (PROJECT / "sources/Engine/src/top/EffectSystem.cpp").read_text()
        loop = "for (const auto& part : document.document.parts) {"
        self.assertEqual(original.count(loop), 1)
        source = self.root / "renderer.cpp"
        source.write_text(original.replace(loop, loop +
                          "\n            if (profile.modelTracks.size() >= " + value + ") break;")
                          .replace("const bool native = !part.billboard && legacy_mesh::recognized(part.model);",
                                   "const bool native = !part.billboard && legacy_mesh::recognized(part.model);\n"
                                   "if (native && resolved.nativeMeshes.size() >= 16) continue;", 1))
        return source

    def test_changed_renderer_limit_is_read_and_missing_limit_is_unknown(self):
        _, rows = self.audit(self.renderer_with_limit("12"))
        self.assertEqual(rows["effect/board.eff"]["nested_compat_limit"], "12")
        self.assertEqual(rows["effect/board.eff"]["potential_nested_truncation"], "0")
        _, rows = self.audit(self.renderer_with_limit("unparsed_limit"))
        self.assertEqual(rows["effect/board.eff"]["nested_compat_limit"], "")
        self.assertEqual(rows["effect/board.eff"]["potential_nested_truncation"], "unknown")

    def test_preview_plan_joins_actual_parent_and_preserves_channel(self):
        output, _ = self.audit(self.renderer_with_limit("8"))
        self.deps.write_text("skill_id,channel,dependency_type,resolved_path_or_status\n"
                             "81,actor,scene_par,Client/effect/a.par\n"
                             "82,impact_target,scene_par,Client/effect/a.par\n"
                             "83,flight,eff,Client/effect/native.eff\n"
                             "84,actor,scene_par,Client/effect/unrelated.par\n")
        destination = self.root / "preview.csv"
        self.assertEqual(build_plan(output / "fx_eff_part_counts.csv", self.deps, destination), 3)
        self.assertEqual(plan(destination), [(81, "cast"), (82, "hit"), (83, "flight")])


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
