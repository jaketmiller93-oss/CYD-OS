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
LINK_PROFILE='crowpanel-v13-c5-p5'

def link_config(board,profile=None):
    if board not in {'nm_cyd_c5','crowpanel_s3'}:
        raise ValueError('Unknown board profile')
    if profile is None:
        return {'CONFIG_CYD_LINK_ENABLE':'n'}
    if profile!=LINK_PROFILE:
        raise ValueError('Unknown physical link profile')
    # Qualified against V1.3 schematic/PCB and the user's connector photos.
    # S3 empty wireless socket J11:6/5; C5 P5:3/2. Common GND, no power wire.
    tx,rx=(8,2) if board=='crowpanel_s3' else (5,4)
    return {'CONFIG_CYD_LINK_ENABLE':'y','CONFIG_CYD_LINK_WIRING_CONFIRMED':'y',
            'CONFIG_CYD_LINK_TX_GPIO':str(tx),'CONFIG_CYD_LINK_RX_GPIO':str(rx),
            'CONFIG_CYD_LINK_BAUD':'115200'}

def verify_link_config(text,expected):
    for key,value in expected.items():
        if value=='n':
            valid=re.search(r'^# '+key+r' is not set$',text,re.M)
        else:
            valid=re.search(r'^'+key+'='+re.escape(value)+r'$',text,re.M)
        if not valid:
            raise ValueError('Physical link configuration mismatch: '+key)

def default_version(board):
    if board not in {'nm_cyd_c5','crowpanel_s3'}:
        raise ValueError('Unknown board profile')
    return (ROOT/'versions'/f'{board}.txt').read_text().strip()

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
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--board',choices=['nm_cyd_c5','crowpanel_s3'],default='nm_cyd_c5');p.add_argument('--version',help='Override this board release line (defaults to versions/<board>.txt)');p.add_argument('--output',type=Path,default=ROOT/'dist')
    p.add_argument('--link-profile',choices=[LINK_PROFILE],help='Enable UART only for the confirmed three-wire, empty-socket S3/C5 cable; see docs/SOLDER-FREE-PAIRING.md')
    a=p.parse_args()
    if a.version is None:a.version=default_version(a.board)
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+(?:[a-zA-Z0-9.-]{0,16})?',a.version) or len(a.version.encode())>=32:p.error('Use a short numeric version with an optional board suffix')
    if not os.getenv('IDF_PATH'):p.error('Activate ESP-IDF 5.5.4 first')
    expected_link=link_config(a.board,a.link_profile)
    profile_suffix='-link-'+a.link_profile if a.link_profile else ''
    build=ROOT/f'build-ota-{a.board}-{a.version}{profile_suffix}';build.mkdir(exist_ok=True)
    config=build/'sdkconfig'
    if config.exists():
        text=config.read_text()
        for key in PRIVATE:
            match=re.search(r'^'+key+r'=(.*)$',text,re.M)
            if match and match[1]!='""':p.error('Dedicated OTA build contains private configuration; use a new version/build folder')
        verify_link_config(text,expected_link)
    defaults=build/'ota.defaults';defaults.write_text('CONFIG_CYD_BOARD_CONFIRMED=y\n'+''.join(key+'=""\n' for key in PRIVATE))
    link_defaults=build/'link.defaults'
    link_defaults.write_text(''.join(key+'='+value+'\n' for key,value in expected_link.items()))
    target='esp32c5' if a.board=='nm_cyd_c5' else 'esp32s3'
    subprocess.run(['idf.py','-B',str(build),'-D','CYD_BOARD='+a.board,'-D','IDF_TARGET='+target,'-D','PROJECT_VER='+a.version,'-D','SDKCONFIG='+str(config),'-D','SDKCONFIG_DEFAULTS=sdkconfig.defaults;'+str(defaults)+';'+str(link_defaults),'build'],cwd=ROOT,check=True)
    text=config.read_text()
    for key in PRIVATE:
        if not re.search(r'^'+key+r'=""$',text,re.M):raise ValueError('Private field is not blank: '+key)
    verify_link_config(text,expected_link)
    data=(build/'cyd-os.bin').read_bytes();metadata=validate_image(data,a.board,a.version)
    metadata['link_profile']=a.link_profile
    metadata['link_config']=expected_link
    a.output.mkdir(parents=True,exist_ok=True);name=f'cyd-os-{a.version}-{a.board}{"-paired" if a.link_profile else ""}-ota.bin';(a.output/name).write_bytes(data);(a.output/(name+'.json')).write_text(json.dumps(metadata,indent=2)+'\n')
    print('Verified OTA application: '+str(a.output/name))
if __name__=='__main__':main()
