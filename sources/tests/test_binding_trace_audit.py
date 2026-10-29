import csv
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from Tools.binding_trace_audit import PREFIX, audit
from Tools.run_binding_previews import plan, run as run_previews

BINARY = sys.argv[1] if len(sys.argv) > 1 else None
PROBE = sys.argv[2] if len(sys.argv) > 2 else None
RENDERER = sys.argv[3] if len(sys.argv) > 3 else None


class BindingTraceAuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.log = self.root / "trace.log"
        self.out = self.root / "out"
        self.candidates = self.root / "candidates.csv"
        self.candidates.write_text("skill_id,channel,effect_id,par,system,part,eff,status,non_billboard_binding_candidate\n"
                                   "81,target,195,effect/a.par,1,2,effect/a.eff,child_not_billboard,1\n")

    def trace(self, **changes):
        row = dict(skill_id=81, visual_id=195, owner="target_actor", system=1, part=2,
                   particle="a.par", resource="a.eff", model="RectPlane", dummy_available=1,
                   bound_matrix=1, bound_center=0, vertices=6)
        row.update({"m" + str(i): 1 if i in (0, 5, 10, 15) else 0 for i in range(16)})
        row.update(changes)
        stream = io.StringIO(); writer = csv.DictWriter(stream, fieldnames=list(row))
        writer.writeheader(); writer.writerow(row)
        self.log.write_text("unrelated renderer message\n" + "".join(PREFIX + line for line in stream.getvalue().splitlines(True)))

    def test_observation_is_scoped_and_never_visual_approval(self):
        self.trace()
        result = audit(self.log, self.out, "preview", self.candidates)
        self.assertEqual(result["candidate_skills_applied"], [81])
        self.assertEqual(result["declared_scope"], "preview")
        self.assertIn("not combat", result["evidence_limit"])
        with (self.out / "binding_observations.csv").open() as f:
            row = next(csv.DictReader(f))
        self.assertEqual(row["visual_validation"], "not_performed")
        self.assertEqual(row["distinct_matrices"], "1")

    def test_no_vertices_or_no_dummy_do_not_prove_application(self):
        for changes in ({"vertices": 0}, {"dummy_available": 0}, {"bound_matrix": 0}):
            self.trace(**changes)
            self.assertEqual(audit(self.log, self.out, "synthetic", self.candidates)["candidate_rows_applied"], 0)

    def test_exact_source_indices_and_filename_are_required(self):
        for changes in ({"system": 0}, {"part": 0}, {"particle": "other.par"}, {"resource": "A.eff"}):
            self.trace(**changes)
            self.assertEqual(audit(self.log, self.out, "synthetic", self.candidates)["candidate_rows_applied"], 0)

    def test_missing_trace_nonfinite_and_client_output_rejected(self):
        self.log.write_text("no trace\n")
        with self.assertRaisesRegex(ValueError, "no binding trace"):
            audit(self.log, self.out, "synthetic")
        self.trace(m0="nan")
        with self.assertRaisesRegex(ValueError, "nonfinite"):
            audit(self.log, self.out, "synthetic")
        self.trace()
        with self.assertRaisesRegex(ValueError, "outside Client"):
            audit(self.log, self.root / "Client/out", "synthetic")

    @unittest.skipUnless(RENDERER, "requires compiled child renderer fixture")
    def test_preview_plan_and_user_isolation_with_fake_renderer(self):
        self.assertEqual(plan(self.candidates), [(81, "hit")])
        client = self.root / "Client"
        (client / "user").mkdir(parents=True)
        original = client / "user/save.txt"
        original.write_text("preserve")
        (client / "effect").mkdir()
        output = self.root / "previews"
        result = run_previews(RENDERER, client, self.candidates, output)
        self.assertTrue(result["original_user_unchanged"])
        self.assertEqual(result["runs"][0]["exit_code"], 0)
        self.assertEqual(original.read_text(), "preserve")
        self.assertEqual((output / "session/Client/user/save.txt").read_text(), "isolated")
        self.assertTrue((output / "session/Client/effect").is_symlink())

    @unittest.skipUnless(BINARY, "requires compiled SK2.18-I fixture executable")
    def test_actual_engine_trace_on_synthetic_par_eff_lgo(self):
        environment = dict(os.environ, TOP_HISTORICAL_BINDING_TRACE="1")
        run = subprocess.run([BINARY], capture_output=True, text=True, env=environment)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.log.write_text(run.stderr)
        result = audit(self.log, self.out, "synthetic")
        self.assertGreater(result["trace_rows"], 0)
        with (self.out / "binding_observations.csv").open() as f:
            rows = list(csv.DictReader(f))
        self.assertTrue(any(int(r["matrix_samples"]) > 0 for r in rows))
        self.assertTrue(any(r["model"] == "mesh.lgo" and int(r["matrix_samples"]) > 0 for r in rows))
        self.assertTrue(any(r["owner"] == "target_actor" for r in rows))

    @unittest.skipUnless(PROBE, "requires compiled CPU skill probe")
    def test_filtered_system_and_child_keep_original_indices(self):
        client = self.root / "Client"
        (client / "effect").mkdir(parents=True)
        (client / "scripts/table").mkdir(parents=True)
        (client / "texture/effect").mkdir(parents=True)
        (client / "texture/effect/bound.tga.png").write_bytes(b"fixture")
        def number(fmt, value): return struct.pack("<" + fmt, value)
        def name(value): return value.encode().ljust(32, b"\0")
        def part(model):
            return (name("child") + struct.pack("<iiifHff", 1, 5, 7, 1., 2, 0., 1.) +
                    struct.pack("<6f", *([1.] * 6)) + struct.pack("<12f", *([0.] * 12)) +
                    struct.pack("<8f", *([1.] * 8)) + struct.pack("<HHfHf", 0, 0, 0., 0, 0.) +
                    name("bound.tga") + name(model) + struct.pack("<Bii3fHffiB4fB", 0, 1, 8, 3., 1., 2., 0, 0., 0., 0, 0, 0., 0., 0., 0., 0))
        # EFF v6: first child fails mesh resolution and is skipped by runtime.
        eff = (struct.pack("<IiB", 6, 0, 0) + name("") + number("B", 0) + name("") +
               number("B", 0) + struct.pack("<4fi", 0., 0., 0., 0., 2) + part("MissingMesh") + part("RectPlane"))
        (client / "effect/child.eff").write_bytes(eff)
        def system(texture, model):
            return (number("i", 6) + name("parent") + number("i", 1) + name(texture) + name(model) +
                    struct.pack("<3fHf3f4fB4i9fB", 0., 0., 0., 1, 1., 0., 0., 0., 1., 1., 1., 1., 0,
                                5, 7, 1, 1, 4., 0., 0., 0., 0., 0., 0., 0., 0., 0) + name("") +
                    struct.pack("<5fBB", 0., 0., 1., 0., 2., 0, 0) + name("") + struct.pack("<iB", 2, 0))
        # First PAR system has neither texture nor .eff and is filtered out.
        (client / "effect/a.par").write_bytes(number("I", 14) + name("controller") + struct.pack("<if", 2, 4.) + system("missing", "") + system("bound.tga", "child.eff"))
        columns = ["0"] * 74; columns[0] = "81"; columns[50] = "1"; columns[51] = "336"
        (client / "scripts/table/skillinfo.txt").write_text("\t".join(columns) + "\n")
        (client / "scripts/table/sceneffectinfo.txt").write_text("336\ta.par\tfixture\n")
        (client / "scripts/table/magicsingleinfo.txt").write_text("38\tdefault\t1\tchild.eff\t10\t0\t0\t-1\t1\t0\t0\n")
        run = subprocess.run([PROBE, str(client), str(self.root / "probe.csv")], capture_output=True, text=True,
                             env=dict(os.environ, TOP_HISTORICAL_BINDING_TRACE="1"))
        self.assertEqual(run.returncode, 0, run.stderr)
        self.log.write_text(run.stderr)
        audit(self.log, self.out, "synthetic")
        with (self.out / "binding_observations.csv").open() as f:
            rows = list(csv.DictReader(f))
        self.assertTrue(rows)
        self.assertTrue(all(r["system"] == "1" and r["part"] == "1" and r["particle"] == "a.par" for r in rows))


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
