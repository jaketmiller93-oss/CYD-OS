#!/usr/bin/env python3
"""Build a standalone stored-ZIP app; does not modify or install firmware."""
from pathlib import Path
import argparse,hashlib,importlib.util,sys
from make_sprites import main as sprites
ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cydos',type=Path,required=True,help='CYDOS 0.4+ source checkout')
    args=parser.parse_args();sprites()
    code='-- Pocket Pal 0.1.0; generated from src/model.lua and src/ui.lua.\n'
    code+='\n'.join((ROOT/'src'/name).read_text() for name in ['model.lua','ui.lua'])
    (ROOT/'app/main.lua').write_text(code)
    spec=importlib.util.spec_from_file_location('cydctl',args.cydos/'tools/cydctl/cydctl.py')
    module=importlib.util.module_from_spec(spec);sys.modules[spec.name]=module;spec.loader.exec_module(module)
    manifest,raw=module.package_project(ROOT/'app')
    metadata=module.inspect_package(raw)
    out=ROOT/'dist';out.mkdir(exist_ok=True)
    target=out/'pocket-pal-0.1.0.cydapp';target.write_bytes(raw)
    (out/'SHA256SUMS.txt').write_text(hashlib.sha256(raw).hexdigest()+'  '+target.name+'\n')
    print(metadata);print(f'{target}: {len(raw)} bytes')
if __name__=='__main__':main()
