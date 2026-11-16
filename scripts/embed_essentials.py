#!/usr/bin/env python3
"""Embed the actual bounded Lua apps directly in a role's firmware."""
from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--role',choices=['host','manager'],required=True);a=p.parse_args()
apps=['calculator','calendar','clock','notes','contacts'] if a.role=='host' else ['files']
common=(a.source/'common.lua').read_bytes();result=['/* Generated from apps/essentials; do not edit. */']
for key in apps:
    code=(common+b'\n' if key!='calculator' else b'')+(a.source/(key+'.lua')).read_bytes()
    result.append(f'static const char app_{key}[] =')
    for off in range(0,len(code),32):result.append('"'+''.join(f'\\x{x:02x}' for x in code[off:off+32])+'"')
    result[-1]+=';'
result.append('static const builtin_t builtins[] = {')
for key in apps:result.append('{"'+key+'","'+key.title()+'",app_'+key+',sizeof(app_'+key+')-1,'+('true' if key!='calculator' else 'false')+','+('true' if key in ['calendar','notes','contacts','files'] else 'false')+'},')
result.append('};');a.output.parent.mkdir(parents=True,exist_ok=True);text='\n'.join(result)+'\n'
if not a.output.exists() or a.output.read_text()!=text:a.output.write_text(text)
