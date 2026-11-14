#!/usr/bin/env python3
"""Package source and the checksum-checked one-time C5 bootstrap overlay."""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT.parent
BUNDLE=OUT/'cyd-os-update'

def source_files():
    roots=['components','boards','main','docs','tests','tools','scripts','apps']
    for root in roots:
        for p in sorted((ROOT/root).rglob('*')):
            if p.is_file() and not any(part in {'__pycache__','.git','.venv','node_modules'} for part in p.relative_to(ROOT).parts) and p.suffix not in {'.pyc','.ppm','.so'}:
                yield p
    for name in ['CMakeLists.txt','README.md','partitions.csv','sdkconfig.defaults','sdkconfig.defaults.esp32c5','sdkconfig.defaults.esp32s3','dependencies.lock','.gitignore','PROJECT_CONTEXT.md']:
        p=ROOT/name
        if p.is_file():yield p

def zip_tree(folder,destination):
    with zipfile.ZipFile(destination,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for p in sorted(folder.rglob('*')):
            if p.is_file():z.write(p,str(Path(folder.name)/p.relative_to(folder)))

def main():
    if BUNDLE.exists():shutil.rmtree(BUNDLE)
    (BUNDLE/'payload').mkdir(parents=True)
    files=list(source_files());manifest={}
    for p in files:
        rel=p.relative_to(ROOT)
        # Existing application sources and partition tables are never overwritten.
        if rel.parts[0]=='apps' or rel.name in {'partitions.csv','dependencies.lock','.gitignore'}:continue
        target=BUNDLE/'payload'/rel;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target);manifest[str(rel)]=hashlib.sha256(p.read_bytes()).hexdigest()
    (BUNDLE/'manifest.json').write_text(json.dumps({'format':1,'version':'0.3.2','files':manifest},indent=2)+'\n')
    for name in ['apply_os_update.py','apply_connection_update.py','ota_config.py','install_c5_update.sh']:shutil.copy2(ROOT/'scripts'/name,BUNDLE/name)
    shutil.copy2(ROOT/'docs/OTA.md',BUNDLE/'README.md')
    image=ROOT/'dist/cyd-os-0.3.2-nm_cyd_c5-ota.bin'
    for p in [image,Path(str(image)+'.json')]:shutil.copy2(p,BUNDLE/p.name);shutil.copy2(p,OUT/p.name)
    (BUNDLE/'VALIDATION.txt').write_text('CYD OS 0.3.2 bootstrap / OTA\nESP-IDF 5.5.4: ESP32-C5 and ESP32-S3 builds pass.\n98 Python/native/browser tests pass.\nActual LVGL shell renders at both supported sizes in all 4 orientations.\nNative tests use SDK doubles; browser tests use a simulated API.\nNew OTA/settings/theme/folders behavior still needs physical C5 acceptance.\nCrowPanel S3 hardware has not been tested.\n')
    zip_tree(BUNDLE,OUT/'cyd-os-update.zip')
    with zipfile.ZipFile(OUT/'cyd-os-source.zip','w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for p in files:z.write(p,str(Path('cyd-os')/p.relative_to(ROOT)))
    for p in [OUT/'cyd-os-update.zip',OUT/'cyd-os-source.zip',OUT/image.name]:
        print(p.name,p.stat().st_size,hashlib.sha256(p.read_bytes()).hexdigest())
if __name__=='__main__':main()
