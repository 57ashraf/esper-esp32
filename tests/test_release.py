import importlib.util
from pathlib import Path
import re
import shutil
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("scan", ROOT/"tools/scan_publication.py")
scan = importlib.util.module_from_spec(spec); spec.loader.exec_module(scan)
class ReleaseTests(unittest.TestCase):
    def test_publication_tree_and_dashboard(self):
        _, errors = scan.scan(ROOT)
        self.assertEqual(errors, [])
    def test_documented_tree_matches_exact_export(self):
        document = (ROOT/"docs/PUBLICATION_TREE.md").read_text(encoding="utf-8")
        tree = document.split("```text\n", 1)[1].split("```", 1)[0].splitlines()
        self.assertEqual(tree[0], "esper-esp32/")
        parents = []
        documented = set()
        for line in tree[1:]:
            match = re.fullmatch(r"((?:│   |    )*)(?:├── |└── )(.+)", line)
            self.assertIsNotNone(match, "Invalid publication tree line")
            depth = len(match.group(1)) // 4
            name = match.group(2)
            parents = parents[:depth]
            self.assertEqual(len(parents), depth, "Missing tree parent")
            if name.endswith("/"):
                parents.append(name[:-1])
            else:
                documented.add("/".join(parents + [name]))
        actual = {p.relative_to(ROOT).as_posix() for p in ROOT.rglob("*")
                  if p.is_file() and ".git" not in p.relative_to(ROOT).parts}
        self.assertEqual(documented, actual)
    def test_release_identity_and_prominent_validation_warning(self):
        readme = (ROOT/"README.md").read_text(encoding="utf-8")
        self.assertTrue(readme.startswith("# esper-esp32 — v0.1.1 (experimental)"))
        introduction = " ".join(readme.split("## What it does", 1)[0].split())
        for required in ("trusted-LAN-only", "@57ashraf", "not been flashed or physically validated"):
            self.assertIn(required, introduction)
        notice = (ROOT/"NOTICE.md").read_text(encoding="utf-8")
        self.assertIn("Copyright (c) 2021 Zach Morris", notice)
        self.assertIn("Copyright (c) 2026 57ashraf", notice)
        self.assertIn("Copyright (c) 2021 Zach Morris", (ROOT/"LICENSE").read_text(encoding="utf-8"))
        self.assertIn('set(PROJECT_VER "0.1.1")', (ROOT/"software/firmware/CMakeLists.txt").read_text(encoding="utf-8"))
    def test_scanner_rejects_private_configs_and_logs(self):
        with tempfile.TemporaryDirectory() as temp:
            copy = Path(temp)/"source"
            shutil.copytree(ROOT, copy)
            for filename in ("sdkconfig.secret", "settings.json", ".env.local", "build.log"):
                fixture = copy/filename
                fixture.write_text("", encoding="utf-8")
                _, errors = scan.scan(copy)
                self.assertIn((filename, "unwanted artifact/config"), errors)
                fixture.unlink()
    def test_scanner_rejects_real_tokens_and_paths(self):
        # Patterns assembled at runtime so this source does not contain fake secret signatures.
        samples = [
            "gh"+"p_"+"x"*36,
            "A"+"KIA"+"X"*16,
            "C:"+chr(92)+"Users"+chr(92)+"private"+chr(92)+"config",
            "-----BEGIN "+"PRIVATE KEY-----",
        ]
        for name, pattern in scan.PATTERNS.items():
            import re
            self.assertIsNotNone(re.compile(pattern))
        import re
        for text in samples:
            self.assertTrue(any(re.search(pattern,text.encode()) for pattern in scan.PATTERNS.values()))
if __name__ == "__main__": unittest.main()
