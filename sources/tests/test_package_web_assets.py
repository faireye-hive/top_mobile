import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import gzip
import hashlib
import base64

spec = importlib.util.spec_from_file_location("package_web_assets", Path(__file__).resolve().parents[1] / "Tools/package_web_assets.py")
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


class WebPackageTests(unittest.TestCase):
    def test_compact_roundtrip_privacy_and_deterministic_blocks(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            client, output = root / 'Client', root / 'public'
            expected = {'map/garner.map': bytes(range(256)) * 10,
                        'map/darkblue.map': b'other map',
                        'texture/Tree.BMP.png': b'pixels' * 400,
                        'texture/Flower.PNG': b'flower' * 100,
                        'scripts/table/skillinfo.txt': b'skills'}
            for name, raw in dict(expected, **{'user/save.txt': b'private', 'texture/backup.zip': b'archive'}).items():
                path = client / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(raw)
            (client / 'texture/link.png').symlink_to(client / 'user/save.txt')
            report = packager.package(client, output, compact=True, pack_bytes=1024)
            index_text = (output / 'assets-index.json').read_bytes()
            index = json.loads(index_text)
            self.assertEqual(index['version'], 2)
            self.assertEqual(report['originalBytes'], sum(map(len, expected.values())))
            decoded = []
            for pack in index['packs']:
                raw = (output / pack['url']).read_bytes()
                self.assertEqual(pack['sha256'], base64.b64encode(hashlib.sha256(raw).digest()).decode())
                decoded.append(gzip.decompress(raw))
            self.assertEqual({f['path'] for f in index['files']}, {'Client/' + p for p in expected})
            for entry in index['files']:
                raw = decoded[entry['pack']][entry['offset']:entry['offset'] + entry['size']]
                self.assertEqual(raw, expected[entry['path'][len('Client/'):]])
            packager.package(client, output, compact=True, pack_bytes=1024)
            self.assertEqual(index_text, (output / 'assets-index.json').read_bytes())
            self.assertTrue((output / '.nojekyll').exists())
            packager.package(client, output, compact=True, maps=['garner'])
            selected = json.loads((output / 'assets-index.json').read_text())
            self.assertNotIn('Client/map/darkblue.map', {f['path'] for f in selected['files']})
    def test_publishes_resources_but_excludes_private_saves_archives_and_links(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            client = root / "Client"
            for name, content in {
                "map/garner.map": b"map",
                "texture/Tree.BMP.png": b"image",
                "scripts/table/skillinfo.txt": b"skills",
                "user/offline-save-1.txt": b"private",
                "texture/old.zip": b"archive",
                "scripts/.secret.txt": b"secret",
                "system/Game.exe": b"executable",
            }.items():
                path = client / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(content)
            (client / "texture/linked.png").symlink_to(client / "user/offline-save-1.txt")
            (client / "scripts/linked").symlink_to(client / "user", target_is_directory=True)
            output = root / "public"
            packager.package(client, output)
            index = json.loads((output / "assets-index.json").read_text())
            self.assertEqual({f["path"] for f in index["files"]}, {
                "Client/map/garner.map", "Client/texture/Tree.BMP.png", "Client/scripts/table/skillinfo.txt"})
            self.assertFalse((output / "assets/Client/user").exists())
            # A release copy must not change when a local resource is edited.
            (client / "map/garner.map").write_bytes(b"new")
            self.assertEqual((output / "assets/Client/map/garner.map").read_bytes(), b"map")
            (client / "texture/Tree.BMP.png").unlink()
            packager.package(client, output)
            self.assertFalse((output / "assets/Client/texture/Tree.BMP.png").exists())

    def test_rejects_output_inside_client(self):
        with tempfile.TemporaryDirectory() as folder:
            client = Path(folder)
            with self.assertRaises(ValueError):
                packager.package(client, client / "public")


if __name__ == "__main__":
    unittest.main()
