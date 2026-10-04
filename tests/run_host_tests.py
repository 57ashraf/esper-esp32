#!/usr/bin/env python3
"""Actual firmware logic on Windows/POSIX; optional Linux ASan/UBSan and real loopback."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import argparse
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "software/tools/blocklist"))
from blocklist_format import build_binary, parse_rules

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--sanitizers",action="store_true",help="ASan/UBSan on supported POSIX Clang")
    args=parser.parse_args()
    if args.sanitizers and os.name=="nt": raise SystemExit("Use the Linux CI sanitizer job, not Windows MinGW.")
    sanitize=["-fsanitize=address,undefined","-fno-omit-frame-pointer"] if args.sanitizers else []
    link=["-lws2_32"] if os.name=="nt" else []
    cxx = os.environ.get("CXX", "clang++")
    cc = os.environ.get("CC", "clang")
    fw = ROOT / "software/firmware/components"
    includes = [ROOT/"tests/shims", ROOT/"tests/vendor/cJSON",
                fw/"dns/include", fw/"http/include", fw/"settings", fw/"flash", fw/"error", fw/"events", fw/"lists"]
    sources = [ROOT/"tests/host_tests.cpp", fw/"dns/dns.cpp", fw/"dns/logging.cpp", fw/"dns/forwarder.cpp", fw/"dns/metrics.cpp",
               fw/"http/safe_dashboard.cpp", fw/"http/get_handlers.cpp", fw/"http/webserver.cpp",
               fw/"settings/settings.cpp", fw/"flash/filesystem.cpp"]
    with tempfile.TemporaryDirectory(prefix="esper-host-") as temp:
        temp = Path(temp)
        obj = temp/"cJSON.o"
        subprocess.run([cc, "-O2", *sanitize, "-c", str(ROOT/"tests/vendor/cJSON/cJSON.c"), "-o", str(obj)], check=True)
        guard = subprocess.run([cxx,"-std=c++17","-DESPER_TEST_COLLISION_HASH",*["-I"+str(p) for p in includes],
                               "-c",str(fw/"lists/lists.cpp"),"-o",str(temp/"forbidden.o")],
                               capture_output=True,text=True)
        assert guard.returncode != 0 and "hash override is forbidden" in guard.stderr
        print("PASS: firmware rejects the host-test-only hash override", flush=True)
        for mode in ("normal", "collision", "logging-off"):
            collisions = mode == "collision"
            data = temp/mode
            data.mkdir()
            kwargs = {"hash_function": lambda _: 42} if collisions else {}
            image, _ = build_binary(parse_rules(["ads.example", "*.wild.example", "track?.foo"]), **kwargs)
            (data/"blocklist.bin").write_bytes(image)
            exe = temp/(mode+(".exe" if os.name=="nt" else ""))
            flags = ["-std=c++17", "-O2", "-g", "-pthread", "-DESPER_HOST_TEST"]
            if mode != "logging-off": flags += ["-DCONFIG_ESPER_QUERY_LOG_ENABLED"]
            if collisions: flags += ["-DESPER_TEST_COLLISION_HASH"]
            subprocess.run([cxx,*flags,*sanitize,*["-I"+str(p) for p in includes],*[str(p) for p in sources],str(obj),*link,"-o",str(exe)],check=True)
            subprocess.run([str(exe),str(data)],check=True)
            if mode == "normal":
                bench = temp/"benchmark"; bench.mkdir()
                rules = parse_rules(f"d{i:05d}.blocked.test" for i in range(35000))
                image, _ = build_binary(rules)
                assert image == build_binary(list(reversed(rules)))[0], "Non-reproducible generator"
                (bench/"blocklist.bin").write_bytes(image)
                subprocess.run([str(exe),str(bench),"--benchmark"],check=True)
        core = fw/"littlefs/src/littlefs"
        core_objects = []
        for name in ("lfs.c", "lfs_util.c"):
            target = temp/(name+".o")
            subprocess.run([cc,"-O2",*sanitize,"-c",str(core/name),"-o",str(target)],check=True)
            core_objects.append(str(target))
        core_exe = temp/"littlefs-core.exe"
        subprocess.run([cxx,"-std=c++17","-O2",*sanitize,str(ROOT/"tests/littlefs_tests.cpp"),*core_objects,"-o",str(core_exe)],check=True)
        subprocess.run([str(core_exe),str(temp/"benchmark/blocklist.bin")],check=True)
        network=temp/("transport.exe" if os.name=="nt" else "transport")
        subprocess.run([cxx,"-std=c++17","-O2","-pthread",*sanitize,"-DESPER_HOST_TEST","-DESPER_REAL_SOCKETS",
                        *["-I"+str(p) for p in includes],str(ROOT/"tests/transport_tests.cpp"),str(fw/"dns/tcp_transport.cpp"),
                        str(fw/"dns/dns.cpp"),*link,"-o",str(network)],check=True)
        subprocess.run([str(network)],check=True,timeout=30)
if __name__ == "__main__":
    main()
