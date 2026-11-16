"""CYD OS package builder and authenticated HTTP client (standard library only)."""
from __future__ import annotations
import argparse
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import socket
import stat
import sys
import time
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode, urlsplit
from urllib.request import Request, build_opener, HTTPRedirectHandler
import zipfile
import struct
import zlib

MAX_PACKAGE = 512 * 1024
MAX_FILE = 64 * 1024
MAX_FILES = 48

class CydError(Exception):
    pass

def safe_path(value):
    return (isinstance(value, str) and 0 < len(value) < 96 and value.count('/') <= 4
            and all(0 < len(p) <= 64 and not p.startswith('.') and not p.endswith('.')
                    and re.fullmatch(r'[a-z0-9_.-]+', p) for p in value.split('/')))

def unique_object(pairs):
    obj = {}
    for key, value in pairs:
        if key in obj:
            raise CydError(f'Duplicate manifest field: {key}')
        obj[key] = value
    return obj

def validate_manifest(raw):
    if isinstance(raw,str):raw=raw.encode('utf-8')
    if len(raw) > 4096:
        raise CydError('Manifest limit is 4096 bytes')
    depth=0;quoted=False;escaped=False
    for byte in raw:
        if quoted:
            if escaped:escaped=False
            elif byte==92:escaped=True
            elif byte==34:quoted=False
        elif byte==34:quoted=True
        elif byte in (91,123):
            depth+=1
            if depth>8:raise CydError('Manifest nesting limit is eight levels')
        elif byte in (93,125):
            depth-=1
            if depth<0:raise CydError('Malformed manifest nesting')
    if quoted or depth:raise CydError('Malformed manifest nesting')
    try:
        m = json.loads(raw, object_pairs_hook=unique_object, parse_constant=lambda value: (_ for _ in ()).throw(CydError('Non-finite JSON number')))
    except (ValueError, UnicodeDecodeError) as exc:
        raise CydError(f'Invalid manifest JSON: {exc}') from exc
    def has_nul(v):
        if isinstance(v, str): return '\x00' in v
        if isinstance(v, list): return any(has_nul(x) for x in v)
        if isinstance(v, dict): return any(has_nul(k) or has_nul(x) for k, x in v.items())
        return False
    if has_nul(m):
        raise CydError('NUL is forbidden in manifest strings')
    if not isinstance(m, dict):
        raise CydError('Manifest must be an object')
    package_id = m.get('package_id')
    if not (isinstance(package_id, str) and 3 <= len(package_id) <= 64
            and '.' in package_id and re.fullmatch(r'[a-z0-9][a-z0-9.-]*[a-z0-9]', package_id)):
        raise CydError('package_id must be a lowercase reverse-domain identifier')
    for field, maximum in [('name', 64), ('version', 32)]:
        v = m.get(field)
        if not isinstance(v, str) or not 0 < len(v.encode()) <= maximum or any(ord(c) < 32 for c in v):
            raise CydError(f'Invalid {field}')
    if type(m.get('api_version')) is not int or m['api_version'] != 1:
        raise CydError('Only API version 1 is supported')
    if not safe_path(m.get('entry_point')):
        raise CydError('Invalid entry_point')
    targets = m.get('target')
    if not isinstance(targets, list) or not targets or any(t not in ('esp32s3', 'esp32c5') for t in targets):
        raise CydError('Invalid target list')
    permissions = m.get('permissions')
    if not isinstance(permissions, list) or any(not isinstance(p,str) or p not in {'storage','network','mqtt','credentials','documents'} for p in permissions):
        raise CydError('Supported permissions: storage, network, mqtt, credentials, documents')
    if 'icon' in m and not safe_path(m['icon']):
        raise CydError('Invalid icon path')
    return m

def package_project(source: Path):
    source = source.resolve()
    if not source.is_dir():
        raise CydError('Project directory not found')
    paths = []
    for root, dirs, files in os.walk(source, followlinks=False):
        dirs[:] = sorted(d for d in dirs if d not in {'.git', '__pycache__', '.venv'})
        for name in dirs + files:
            path = Path(root) / name
            if path.is_symlink():
                raise CydError(f'Symlinks are forbidden: {path.relative_to(source)}')
        for name in sorted(files):
            path = Path(root) / name
            relative = path.relative_to(source).as_posix()
            if not safe_path(relative) or relative == 'package.cydapp':
                raise CydError(f'Unsupported or reserved path: {relative}')
            if not path.is_file() or path.stat().st_size > MAX_FILE:
                raise CydError(f'File exceeds 64 KiB or is not regular: {relative}')
            paths.append((relative, path))
    if not 1 <= len(paths) <= MAX_FILES:
        raise CydError('Package requires 1–48 files')
    content = {name: path.read_bytes() for name, path in paths}
    if any(len(data) > MAX_FILE for data in content.values()):
        raise CydError('A file grew beyond the size limit')
    m = validate_manifest(content.get('manifest.json', b''))
    if not content.get(m['entry_point']):
        raise CydError('Entry point is missing or empty')
    if m.get('icon') and m['icon'] not in content:
        raise CydError('Icon file missing')
    output = io.BytesIO()
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_STORED, allowZip64=False) as archive:
        for name, data in sorted(content.items()):
            info = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
            info.create_system = 3
            info.external_attr = (stat.S_IFREG | 0o644) << 16
            archive.writestr(info, data)
    result = output.getvalue()
    if len(result) > MAX_PACKAGE:
        raise CydError('Package exceeds 512 KiB')
    return m, result

def inspect_package(raw):
    """Mirror the firmware's strict stored-ZIP layout, flags, paths and CRC policy."""
    if not 22 <= len(raw) <= MAX_PACKAGE:raise CydError('Package size limit')
    def u16(p):return struct.unpack_from('<H',raw,p)[0]
    def u32(p):return struct.unpack_from('<I',raw,p)[0]
    end=len(raw)-22
    if u32(end)!=0x06054b50 or u16(end+4) or u16(end+6) or u16(end+20):raise CydError('Invalid ZIP end record')
    count=u16(end+10);pos=u32(end+16);cd=pos;size=u32(end+12);local=0;files={}
    if not 1<=count<=MAX_FILES or u16(end+8)!=count or pos+size!=end:raise CydError('Invalid ZIP directory')
    try:
        for _ in range(count):
            if pos+46>end or u32(pos)!=0x02014b50:raise CydError('Invalid central record')
            ns,extra,comment=u16(pos+28),u16(pos+30),u16(pos+32);n=u32(pos+24);mode=u32(pos+38)>>16
            if u16(pos+8) or u16(pos+10) or u16(pos+34) or u32(pos+20)!=n or n>MAX_FILE or not 0<ns<96 or pos+46+ns+extra+comment>end or u32(pos+42)!=local or (mode and stat.S_IFMT(mode)!=stat.S_IFREG) or u32(pos+38)&0x10:raise CydError('Unsupported ZIP entry')
            name=raw[pos+46:pos+46+ns].decode('ascii')
            if not safe_path(name) or name=='package.cydapp' or any(name==old or name.startswith(old+'/') or old.startswith(name+'/') for old in files):raise CydError('Invalid, duplicate or reserved path')
            if local+30>cd or u32(local)!=0x04034b50 or u16(local+6) or u16(local+8) or u32(local+14)!=u32(pos+16) or u32(local+18)!=n or u32(local+22)!=n or u16(local+26)!=ns or raw[local+30:local+30+ns]!=name.encode():raise CydError('Local/central header mismatch')
            offset=local+30+ns+u16(local+28)
            if offset+n>cd:raise CydError('Truncated entry')
            data=raw[offset:offset+n]
            if zlib.crc32(data)!=u32(pos+16):raise CydError('CRC mismatch')
            files[name]=data;local=offset+n;pos+=46+ns+extra+comment
    except (struct.error,UnicodeError) as exc:raise CydError('Malformed ZIP') from exc
    if pos!=end or local!=cd:raise CydError('ZIP layout mismatch')
    m=validate_manifest(files.get('manifest.json',b''))
    if not files.get(m['entry_point']):raise CydError('Missing entry point')
    return m

class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        # Never forward the deployment credential to a redirect destination.
        raise CydError('Device redirected request; refusing to forward token')

class Client:
    def __init__(self, device, token, timeout=20):
        self.url = device if '://' in device else 'http://' + device
        parsed = urlsplit(self.url)
        if (parsed.scheme not in {'http', 'https'} or not parsed.hostname or parsed.username
                or parsed.password or parsed.path not in {'', '/'} or parsed.query or parsed.fragment):
            raise CydError('Device must be a hostname, IP address, or HTTP(S) origin')
        if not token or '\n' in token or '\r' in token:
            raise CydError('Set CYD_TOKEN or use --token-file')
        self.url = self.url.rstrip('/')
        self.token, self.timeout = token, timeout
        self.opener = build_opener(NoRedirect())

    def request(self, endpoint, method='GET', data=None, binary=False):
        headers = {'Authorization': 'Bearer ' + self.token}
        if data is not None:
            headers['Content-Type'] = 'application/zip'
        req = Request(self.url + '/api/' + endpoint, data=data, method=method, headers=headers)
        try:
            with self.opener.open(req, timeout=self.timeout) as response:
                raw = response.read(MAX_PACKAGE + 16384)
            if binary:
                return raw
            return json.loads(raw)
        except HTTPError as exc:
            detail = exc.read(4096)
            try:
                detail = json.loads(detail).get('error', str(exc.code))
            except (ValueError, AttributeError):
                body = detail.decode('utf-8', errors='replace').strip()
                body = body.replace(self.token, '[redacted]')
                detail = (f'HTTP {exc.code} ({method} /api/{endpoint}): '
                          + (body[:512] or 'Empty error response'))
            raise CydError(str(detail)) from exc
        except (URLError, TimeoutError, ValueError) as exc:
            raise CydError(f'Device request failed: {exc}') from exc

    def run(self, revision, wait=10):
        if not re.fullmatch('[a-f0-9]{16}', revision):
            raise CydError('Expected a 16-character revision ID from versions')
        self.request('run?' + urlencode({'revision': revision}), 'POST', b'')
        deadline = time.monotonic() + wait
        while time.monotonic() < deadline:
            status = self.request('status')['app']
            if status['revision'] == revision:
                if status['state'] == 'running':
                    return status
                if status['state'] in {'crashed', 'stopped'}:
                    raise CydError('Startup failed: ' + status.get('error', status['state']))
            time.sleep(0.1)
        raise CydError('Startup was not confirmed before timeout; inspect status and logs')

def parser():
    p = argparse.ArgumentParser(prog='cydctl', description=__doc__)
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument('--device', default=os.getenv('CYD_DEVICE', 'cyd.local'))
    common.add_argument('--token-file', type=Path)
    common.add_argument('--json', action='store_true')
    sub = p.add_subparsers(dest='command', required=True)
    for command in ['devices', 'status', 'projects', 'versions', 'stop']:
        sub.add_parser(command, parents=[common])
    pack = sub.add_parser('pack', parents=[common])
    pack.add_argument('source', type=Path)
    pack.add_argument('-o', '--output', required=True, type=Path)
    inspect = sub.add_parser('inspect',parents=[common]);inspect.add_argument('source',type=Path)
    deploy = sub.add_parser('deploy', parents=[common])
    deploy.add_argument('source', type=Path)
    deploy.add_argument('--run', action='store_true')
    deploy.add_argument('--logs', action='store_true')
    run = sub.add_parser('run', parents=[common]); run.add_argument('revision')
    logs = sub.add_parser('logs', parents=[common]); logs.add_argument('--follow', action='store_true')
    for command in ['download', 'delete']:
        cmd = sub.add_parser(command, parents=[common]); cmd.add_argument('revision')
        if command == 'download': cmd.add_argument('-o', '--output', type=Path, required=True)
    return p

def execute(args):
    if args.command == 'inspect':
        return package_project(args.source)[0] if args.source.is_dir() else inspect_package(args.source.read_bytes())
    if args.command == 'pack':
        m, data = package_project(args.source)
        args.output.write_bytes(data)
        return {'package_id': m['package_id'], 'bytes': len(data), 'path': str(args.output)}
    if args.command == 'devices':
        try: return sorted({s[4][0] for s in socket.getaddrinfo(urlsplit('http://' + args.device).hostname, 80)})
        except socket.gaierror as exc: raise CydError('mDNS lookup failed; use --device with the IP shown on the screen') from exc
    token = args.token_file.read_text().strip() if args.token_file else os.getenv('CYD_TOKEN', '')
    client = Client(args.device, token)
    command = args.command
    if command in {'projects', 'versions'}: return client.request('versions')
    if command == 'status': return client.request('status')
    if command == 'stop': return client.request('stop', 'POST', b'')
    if command == 'run': return client.run(args.revision)
    if command in {'download', 'delete'}:
        if not re.fullmatch('[a-f0-9]{16}', args.revision): raise CydError('Invalid revision ID')
        query = '?' + urlencode({'revision': args.revision})
        if command == 'delete': return client.request('versions' + query, 'DELETE')
        args.output.write_bytes(client.request('download' + query, binary=True))
        return {'path': str(args.output)}
    if command == 'deploy':
        manifest, package = package_project(args.source)
        if client.request('status')['target'] not in manifest['target']:
            raise CydError('Package does not support this device target')
        result = client.request('packages', 'POST', package)
        if args.run: result['app'] = client.run(result['revision'])
        if args.logs: result['logs'] = client.request('logs')
        return result
    if command == 'logs':
        previous = []
        while True:
            entries = client.request('logs')
            if not args.follow: return entries
            overlap = min(len(previous), len(entries))
            while overlap and previous[-overlap:] != entries[:overlap]: overlap -= 1
            for entry in entries[overlap:]:
                print(json.dumps({'log': entry}) if args.json else entry, flush=True)
            previous = entries
            time.sleep(1)

def main(argv=None):
    args = parser().parse_args(argv)
    try:
        result = execute(args)
        print(json.dumps(result, ensure_ascii=False, indent=None if args.json else 2))
        return 0
    except (CydError, OSError) as exc:
        if args.json: print(json.dumps({'error': str(exc)}))
        else: print(f'cydctl: {exc}', file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130

if __name__ == '__main__':
    sys.exit(main())
