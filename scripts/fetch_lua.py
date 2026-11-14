#!/usr/bin/env python3
"""Fetch official Lua 5.4.8 source with a pinned SHA-256; no system installation."""
import hashlib
import io
from pathlib import Path
import tarfile
from urllib.request import urlopen
from urllib.error import URLError
import time
ROOT = Path(__file__).resolve().parents[1]
VERSION = '5.4.8'
SHA256 = '4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae'
for attempt in range(3):
    try:
        data = urlopen(f'https://www.lua.org/ftp/lua-{VERSION}.tar.gz', timeout=20).read()
        break
    except (URLError, TimeoutError):
        if attempt == 2:
            raise
        time.sleep(2)
if hashlib.sha256(data).hexdigest() != SHA256:
    raise SystemExit('Lua source digest mismatch')
with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as archive:
    for m in archive.getmembers():
        prefix = f'lua-{VERSION}/src/'
        if m.isfile() and m.name.startswith(prefix):
            name = m.name[len(prefix):]
            if '/' in name or name in {'lua.c', 'luac.c'}:
                continue
            dest = ROOT / 'components/lua/vendor' / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(archive.extractfile(m).read())
print('Verified Lua', VERSION)
