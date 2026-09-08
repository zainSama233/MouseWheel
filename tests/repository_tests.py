"""Repository publication checks use temporary files, never user settings."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("repository_check", Path(__file__).resolve().parents[1] / "scripts/check_repository.py")
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)

class RepositoryTests(unittest.TestCase):
    def test_private_and_generated_files_are_rejected(self):
        for path in [".env", ".env.local", "demo/config.json", "keys/id_ed25519", "keys/private.pem", "test.flac", "dist/app.exe", "build/result.txt", "AGENTS.MD"]:
            with self.subTest(path=path):
                self.assertTrue(check.forbidden_path(path))

    def test_source_and_intentional_assets_are_allowed(self):
        for path in ["src/config/config_store.cpp", "src/ui/locales/catalog.json", "tests/fixtures/library.svg", "docs/images/settings.png", "toolchain.json", "LICENSE"]:
            with self.subTest(path=path):
                self.assertFalse(check.forbidden_path(path))

    def test_relative_document_links_and_missing_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "README.md").write_text("[guide](docs/guide.md#use) ![UI](docs/ui.png) [web](https://example.com)", encoding="utf-8")
            (root / "docs").mkdir()
            (root / "docs/guide.md").write_text("# Use", encoding="utf-8")
            self.assertEqual(check.validate(root, ["README.md", "docs/guide.md"]), ["README.md: missing local target docs/ui.png"])
            (root / "docs/ui.png").write_bytes(b"test image")
            self.assertEqual(check.validate(root, ["README.md", "docs/guide.md", "docs/ui.png"]), [])

if __name__ == "__main__":
    unittest.main()
