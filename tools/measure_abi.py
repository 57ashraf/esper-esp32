#!/usr/bin/env python3
"""Compile a target-size probe using the validated build; never flash or execute it."""
from pathlib import Path
import argparse
import json
import os
import shlex
import struct
import subprocess
import tempfile

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("build",type=Path)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    commands=json.loads((args.build/"compile_commands.json").read_text(encoding="utf-8"))
    entry=next(e for e in commands if e["file"].replace("\\","/").endswith("/dns/server.cpp"))
    original=shlex.split(entry["command"], posix=os.name != "nt")
    original=[v[1:-1] if v.startswith('"') and v.endswith('"') else v for v in original]
    with tempfile.TemporaryDirectory(prefix="esper-abi-") as t:
        obj=Path(t)/"sizes.o"
        command=[]; i=0
        while i<len(original):
            value=original[i]
            if value.startswith("-DIDF_VER="):
                i+=1;continue
            if value in ("-o","-MF","-MT","-c"):
                i+=2; continue
            if value in ("-MD","-MMD"):
                i+=1;continue
            command.append(value);i+=1
        command+=["-c",str(root/"tools/abi_sizes.cpp"),"-o",str(obj)]
        subprocess.run(command,cwd=entry["directory"],check=True)
        from elftools.elf.elffile import ELFFile
        with obj.open("rb") as f:
            elf=ELFFile(f)
            symbol=elf.get_section_by_name(".symtab").get_symbol_by_name("esper_abi_sizes")[0]
            section=elf.get_section(symbol["st_shndx"])
            offset=symbol["st_value"]
            sizes=struct.unpack("<10I",section.data()[offset:offset+40])
        names=["DNS","Client","Log_Entry","cJSON","std_string","byte_vector","Header","Question","DnsForwarder","DnsMetrics"]
        print(json.dumps(dict(zip(names,sizes)),indent=2))
if __name__=="__main__":main()
