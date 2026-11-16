#!/usr/bin/env python3
"""Build CrowPanel OTA + merged bootstrap without reading device/private config or flashing."""
import argparse, hashlib, json, os, subprocess, sys
from pathlib import Path
from build_ota import PRIVATE, validate_image, default_version
ROOT = Path(__file__).resolve().parents[1]
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', default=default_version('crowpanel_s3'))
    parser.add_argument('--output', type=Path, default=ROOT/'dist/crowpanel')
    args = parser.parse_args()
    if not os.getenv('IDF_PATH'): parser.error('Activate ESP-IDF 5.5.4 first')
    output = args.output.resolve()
    subprocess.run([sys.executable, str(ROOT/'scripts/build_ota.py'), '--board', 'crowpanel_s3',
                    '--version', args.version, '--output', str(output)], check=True)
    build = ROOT/f'build-ota-crowpanel_s3-{args.version}'
    config = (build/'sdkconfig').read_text().splitlines()
    if any(key+'=""' not in config for key in PRIVATE): raise ValueError('Private build fields must be blank')
    if 'CONFIG_CYD_LINK_ENABLE=y' in config: raise ValueError('Release must leave the unqualified UART link disabled')
    app = (build/'cyd-os.bin').read_bytes()
    validate_image(app, 'crowpanel_s3', args.version)
    flash = json.loads((build/'flasher_args.json').read_text())
    files = []
    for offset, filename in flash['flash_files'].items(): files.extend([offset, str(build/filename)])
    merged = output/f'cyd-os-{args.version}-crowpanel_s3-merged.bin'
    subprocess.run([sys.executable, '-m', 'esptool', '--chip', 'esp32s3', 'merge_bin', '-o', str(merged),
                    *flash['write_flash_args'], *files], check=True)
    data = merged.read_bytes()
    for offset, filename in flash['flash_files'].items():
        image = (build/filename).read_bytes(); start = int(offset, 0)
        if data[start:start+len(image)] != image: raise ValueError('Merged segment mismatch: '+filename)
    metadata = dict(board='crowpanel_s3', version=args.version, kind='merged USB bootstrap',
                    flash_address='0x0', bytes=len(data), file_sha256=hashlib.sha256(data).hexdigest(),
                    preserves_existing_settings=False, link_enabled=False, hardware_validated=False,
                    note='Contains bootloader, partition table, initial OTA data and application. Never upload as OTA.')
    merged.with_suffix('.bin.json').write_text(json.dumps(metadata, indent=2)+'\n')
    print('Verified merged CrowPanel bootstrap:', merged)
if __name__ == '__main__': main()
