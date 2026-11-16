#!/usr/bin/env python3
"""Apply CYD OS 0.4.0 source and enable rollback in the existing C5 configuration."""
import argparse
from datetime import datetime
import os
from pathlib import Path
import shutil
import tempfile
from apply_connection_update import apply_update
from ota_config import configured_text
ROOTS={'components','boards','main','docs','tests','tools','scripts','CMakeLists.txt','README.md','PROJECT_CONTEXT.md','sdkconfig.defaults','sdkconfig.defaults.esp32c5','sdkconfig.defaults.esp32s3'}

def bootstrap(bundle,project,config=None):
    project=project.resolve()
    config=(config or project/'build-device-c5/sdkconfig').absolute()
    if config.is_symlink() or not config.resolve().is_relative_to(project):
        raise ValueError('Device configuration must be a regular file inside this source folder')
    original=config.read_text()
    if 'CONFIG_IDF_TARGET_ESP32C5=y' not in original:
        raise ValueError('Choose the existing ESP32-C5 device sdkconfig')
    if 'CONFIG_CYD_BOARD_CONFIRMED=y' not in original:
        raise ValueError('This installer is for your confirmed NM-CYD-C5 hardware')
    updated=configured_text(original)
    backup,count=apply_update(bundle,project,allowed_roots=ROOTS,backup_prefix='.os-update-backup-')
    saved=backup/'private-device-sdkconfig'
    shutil.copy2(config,saved);saved.chmod(0o600)
    temporary=None
    try:
        with tempfile.NamedTemporaryFile(mode='w',dir=config.parent,delete=False) as f:
            temporary=Path(f.name);f.write(updated);f.flush();os.fsync(f.fileno())
        temporary.chmod(0o600);temporary.replace(config)
    finally:
        if temporary and temporary.exists():temporary.unlink()
    return backup,count

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('project',type=Path);p.add_argument('--config',type=Path);args=p.parse_args()
    try:backup,count=bootstrap(Path(__file__).resolve().parent,args.project,args.config)
    except (OSError,ValueError,KeyError,TypeError) as e:p.exit(1,f'Update failed: {e}\n')
    print(f'CYD OS 0.4.0: updated {count} source files. Private backup: {backup}')
    print('Your device Wi-Fi/token/calibration values and application sources were retained.')
    print('Rollback and UI fonts are enabled. Build and USB-flash once using build-device-c5.')
if __name__=='__main__':main()
