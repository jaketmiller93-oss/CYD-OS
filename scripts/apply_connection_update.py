#!/usr/bin/env python3
"""Apply the adjacent CYD connection-manager payload without replacing device configs."""
import argparse
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import tempfile


def apply_update(bundle, project, *, allowed_roots=None, backup_prefix=".connection-manager-backup-"):
    if allowed_roots is None:
        allowed_roots = {"components", "main", "docs", "tests", "tools", "scripts", "CMakeLists.txt", "README.md"}
    project = project.resolve()
    if not (project / 'CMakeLists.txt').is_file() or not (project / 'components/network/network.c').is_file():
        raise ValueError('Choose the existing cyd-os source folder')
    manifest = json.loads((bundle / 'manifest.json').read_text())
    files = manifest.get('files')
    if manifest.get('format') != 1 or not isinstance(files, dict) or not files:
        raise ValueError('Invalid update manifest')
    checked = []
    for name, digest in files.items():
        rel = PurePosixPath(name)
        if (rel.is_absolute() or not rel.parts or '..' in rel.parts or '\\' in name
                or rel.parts[0] not in allowed_roots
                or any(part.startswith('build') for part in rel.parts[:-1])
                or rel.name in {'sdkconfig', 'sdkconfig.old', 'sdkconfig.local'}):
            raise ValueError('Update tried to replace a protected or invalid path')
        source = bundle / 'payload' / name
        target = project / name
        if not source.resolve().is_relative_to((bundle / 'payload').resolve()) or source.is_symlink():
            raise ValueError('Invalid payload location')
        data = source.read_bytes()
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError(f'Payload checksum mismatch: {name}')
        if not target.resolve().is_relative_to(project) or target.is_symlink():
            raise ValueError(f'Target is a symlink or outside the project: {name}')
        checked.append((name, target, data))
    backup = project / (backup_prefix + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    backup.mkdir(mode=0o700)
    existed = {}
    # Back up all existing files before replacing any of them.
    for name, target, _ in checked:
        existed[name] = target.exists()
        if target.exists():
            if not target.is_file():
                raise ValueError(f'Target is not a regular file: {name}')
            saved = backup / name
            saved.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(target, saved)
    (backup / 'files.json').write_text(json.dumps(existed, indent=2) + '\n')
    for _, target, data in checked:
        target.parent.mkdir(parents=True, exist_ok=True)
        mode = stat.S_IMODE(target.stat().st_mode) if target.exists() else 0o644
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as output:
                temporary = Path(output.name)
                output.write(data)
                output.flush()
                os.fsync(output.fileno())
            temporary.chmod(mode)
            temporary.replace(target)
        finally:
            if temporary and temporary.exists():
                temporary.unlink()
    return backup, len(checked)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('project', type=Path, help='Existing cyd-os directory')
    args = parser.parse_args()
    try:
        backup, count = apply_update(Path(__file__).resolve().parent, args.project)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        parser.exit(1, f'Update failed: {exc}\n')
    print(f'Updated {count} source files. Backup: {backup}')
    print('Device sdkconfig files, application sources, and board calibration were preserved.')
    print('Build and flash the C5 image using your existing build-device-c5 configuration.')


if __name__ == '__main__':
    main()
