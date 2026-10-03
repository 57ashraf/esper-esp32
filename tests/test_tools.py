import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"software/tools/blocklist"))
from blocklist_format import build_binary, parse_rules, read_metadata

def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value
generator=module("generate",ROOT/"software/tools/blocklist/generate_blocklist.py")

class ToolTests(unittest.TestCase):
    def test_atomic_generation_and_preserved_output(self):
        with tempfile.TemporaryDirectory() as t:
            t=Path(t); source=t/"input.txt"; source.write_text("ads.example\n",encoding="utf-8")
            output=t/"output.bin"
            old=sys.argv
            try:
                sys.argv=["generate",str(source),str(output)]
                with contextlib.redirect_stdout(io.StringIO()): self.assertEqual(generator.main(),0)
                before=output.read_bytes(); self.assertEqual(read_metadata(before).entry_count,1)
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit): generator.main()
                self.assertEqual(output.read_bytes(),before)
                self.assertFalse(list(t.glob(".esbl-*")))
            finally: sys.argv=old
    def test_capacity_failure_does_not_create_output(self):
        with tempfile.TemporaryDirectory() as t:
            t=Path(t); source=t/"input.txt";source.write_text("ads.example\n",encoding="utf-8")
            old=sys.argv
            try:
                sys.argv=["generate",str(source),str(t/"output.bin"),"--littlefs-bytes","10"]
                with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):generator.main()
                self.assertFalse((t/"output.bin").exists())
            finally:sys.argv=old
    def test_corrupt_hash_reserved_string(self):
        image,_=build_binary(parse_rules(["ads.example"]))
        for offset in (40,55,56):
            bad=bytearray(image);bad[offset]^=255
            with self.assertRaises((ValueError,UnicodeDecodeError)):read_metadata(bytes(bad))
    def test_prepare_first_install_then_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as t:
            t=Path(t)
            config=t/"sdkconfig"
            config.write_text('CONFIG_WIFI_SSID="fixture-ssid"\nCONFIG_WIFI_PASSWORD="fixture-password"\n',encoding="utf-8")
            image,_=build_binary(parse_rules(["ads.example"]));(t/"list.bin").write_bytes(image)
            command=[sys.executable,"-B",str(ROOT/"tools/prepare_filesystem.py"),"--sdkconfig",str(config),
                "--blocklist",str(t/"list.bin"),"--output",str(t/"new-fs")]
            result=subprocess.run(command,capture_output=True,text=True)
            self.assertEqual(result.returncode,0);self.assertNotIn("fixture-",result.stdout+result.stderr)
            file=t/"new-fs/ota_0/settings.json"
            before=file.read_bytes()
            self.assertEqual(json.loads(before)["ssid"],"fixture-ssid")
            result=subprocess.run(command,capture_output=True,text=True)
            self.assertNotEqual(result.returncode,0);self.assertEqual(file.read_bytes(),before)
if __name__=="__main__":unittest.main()
