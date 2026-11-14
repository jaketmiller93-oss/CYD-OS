#!/usr/bin/env python3
"""Build a credential-free application image for subsequent Safari OTA updates."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
ROOT=Path(__file__).resolve().parents[1]
PRIVATE=['CONFIG_CYD_API_TOKEN','CONFIG_CYD_WIFI_SSID','CONFIG_CYD_WIFI_PASSWORD','CONFIG_CYD_AP_PASSWORD']

def validate_image(data,board,version):
    chip=0x17 if board=='nm_cyd_c5' else 9
    if len(data)<512 or len(data)>0x700000 or data[0]!=0xe9 or struct.unpack_from('<H',data,12)[0]!=chip or data[23]!=1:
        raise ValueError('Not a supported application image')
    if data[80:112].split(b'\0')[0]!=b'cyd-os' or data[48:80].split(b'\0')[0].decode()!=version:
        raise ValueError('Unexpected application/version descriptor')
    d=struct.unpack_from('<16sI24sIIIII65s63s',data,288)
    if d[0].rstrip(b'\0')!=b'CYD-OS-UPDATE' or d[1]!=1 or d[2].rstrip(b'\0').decode()!=board or d[3]&7!=7 or d[4:8]!=(1,1,1,1):
        raise ValueError('Missing supported CYD OTA/settings descriptor')
    layout=hashlib.sha256((ROOT/'partitions.csv').read_bytes()).hexdigest()
    if d[8].rstrip(b'\0').decode()!=layout:raise ValueError('Partition layout descriptor mismatch')
    # ESP image hash covers header, segments and checksum padding, excluding the appended hash.
    offset=24
    for _ in range(data[1]):
        if offset+8>len(data):raise ValueError('Truncated segment header')
        length=struct.unpack_from('<I',data,offset+4)[0];offset+=8+length
        if offset>len(data):raise ValueError('Truncated segment')
    hash_at=(offset+16)//16*16
    if hash_at+32!=len(data) or hashlib.sha256(data[:hash_at]).digest()!=data[hash_at:]:raise ValueError('ESP image SHA-256 mismatch')
    return {'board':board,'version':version,'bytes':len(data),'file_sha256':hashlib.sha256(data).hexdigest(),'esp_image_sha256':data[hash_at:].hex(),'partition_layout_sha256':layout}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--board',choices=['nm_cyd_c5','crowpanel_s3'],default='nm_cyd_c5');p.add_argument('--version',default='0.4.0');p.add_argument('--output',type=Path,default=ROOT/'dist');a=p.parse_args()
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+(?:[a-zA-Z0-9.-]{0,16})?',a.version) or len(a.version.encode())>=32:p.error('Use a short semantic version')
    if not os.getenv('IDF_PATH'):p.error('Activate ESP-IDF 5.5.4 first')
    build=ROOT/f'build-ota-{a.board}-{a.version}';build.mkdir(exist_ok=True)
    config=build/'sdkconfig'
    if config.exists():
        text=config.read_text()
        for key in PRIVATE:
            match=re.search(r'^'+key+r'=(.*)$',text,re.M)
            if match and match[1]!='""':p.error('Dedicated OTA build contains private configuration; use a new version/build folder')
    defaults=build/'ota.defaults';defaults.write_text('CONFIG_CYD_BOARD_CONFIRMED=y\n'+''.join(key+'=""\n' for key in PRIVATE))
    target='esp32c5' if a.board=='nm_cyd_c5' else 'esp32s3'
    subprocess.run(['idf.py','-B',str(build),'-D','CYD_BOARD='+a.board,'-D','IDF_TARGET='+target,'-D','PROJECT_VER='+a.version,'-D','SDKCONFIG='+str(config),'-D','SDKCONFIG_DEFAULTS=sdkconfig.defaults;'+str(defaults),'build'],cwd=ROOT,check=True)
    text=config.read_text()
    for key in PRIVATE:
        if not re.search(r'^'+key+r'=""$',text,re.M):raise ValueError('Private field is not blank: '+key)
    data=(build/'cyd-os.bin').read_bytes();metadata=validate_image(data,a.board,a.version)
    a.output.mkdir(parents=True,exist_ok=True);name=f'cyd-os-{a.version}-{a.board}-ota.bin';(a.output/name).write_bytes(data);(a.output/(name+'.json')).write_text(json.dumps(metadata,indent=2)+'\n')
    print('Verified OTA application: '+str(a.output/name))
if __name__=='__main__':main()
