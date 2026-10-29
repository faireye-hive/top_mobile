import csv
import json
import tempfile
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from Tools.particle_rng_audit import audit


class ParticleRngAuditTests(unittest.TestCase):
    def test_ignores_comments_and_literals_and_reports_backend_counts(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            original = root / "original"
            gl = root / "gl"
            (original / "Client/src").mkdir(parents=True)
            (original / "Engine/include").mkdir(parents=True)
            (original / "Libraries/MaxSDK/include").mkdir(parents=True)
            (original / "Libraries/Util/logvwr").mkdir(parents=True)
            (gl / "include/top").mkdir(parents=True)
            (original / "Client/src/random.cpp").write_text(
                '// srand(0); rand();\n'
                'const char* s = "rand()";\n'
                'void seed() { srand(17); }\n'
                'int particle() { return rand() % 2; }\n', encoding="utf-8")
            (original / "Engine/include/MPParticleSys.h").write_text(
                'inline float sample() { return Randf(2.0f); }\n', encoding="utf-8")
            (original / "Libraries/MaxSDK/include/rand.h").write_text(
                'CoreExport void srand(unsigned int seed);\n', encoding="utf-8")
            (original / "Libraries/Util/logvwr/tool.cpp").write_text(
                'void repaint() { srand(1); }\n', encoding="utf-8")
            (gl / "include/top/HistoricalParticleRandom.h").write_text(
                'auto x = dx9Randf(2.f, next);\n'
                'auto y = deterministicRandBits(seed, lane);\n', encoding="utf-8")
            out = root / "out"
            summary = audit(original, gl, out)
            self.assertEqual(summary["counts"].get(
                "DX9:Client:srand:runtime_candidate"), 1)
            self.assertEqual(summary["counts"].get(
                "DX9:Client:rand:runtime_candidate"), 1)
            self.assertEqual(summary["counts"].get(
                "DX9:Libraries:srand:declaration"), 1)
            self.assertEqual(summary["counts"].get(
                "DX9:Libraries:srand:tool_source"), 1)
            self.assertEqual(summary["counts"].get(
                "OpenGL:Engine:rand:runtime_candidate"), None)
            self.assertEqual(summary["counts"].get(
                "OpenGL:Engine:deterministic_helper:runtime_candidate"), 2)
            with (out / "particle_rng_sites.csv").open(newline="") as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual([(row["kind"], row["classification"])
                              for row in rows if row["backend"] == "DX9"],
                             [("srand", "runtime_candidate"), ("rand", "runtime_candidate"),
                              ("srand", "declaration"), ("srand", "tool_source")])
            self.assertTrue((out / "particle_rng_summary.json").is_file())
            data = json.loads((out / "particle_rng_summary.json").read_text())
            self.assertIn("does not prove runtime", data["evidence_limit"])


if __name__ == "__main__":
    unittest.main()
