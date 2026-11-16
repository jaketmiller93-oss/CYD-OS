#!/usr/bin/env python3
"""One logical firmware release with a validated image for each tablet role."""
import argparse,hashlib,importlib.util,json,re,struct,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('pair_ota',ROOT/'scripts/build_ota.py');ota=importlib.util.module_from_spec(spec);spec.loader.exec_module(ota)
MAGIC=b'CYD-PAIR-UPDATE\0';HEADER=512;LIMIT=512+2*0x700000

def package(s3,c5,version):
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+(?:[a-zA-Z0-9.-]{0,16})?',version) or len(version.encode())>=32:raise ValueError('Use a paired release version starting at 0.5.0')
    if tuple(map(int,re.match(r'(\d+)\.(\d+)\.(\d+)',version).groups()))<(0,5,0):raise ValueError('Paired releases start at 0.5.0')
    images=[('crowpanel_s3',bytes(s3)),('nm_cyd_c5',bytes(c5))]
    metadata=[ota.validate_image(data,board,version) for board,data in images]
    for i,(board,data) in enumerate(images):
        if not struct.unpack_from('<I',data,332)[0]&8 or data[417]!=1 or data[418]!=i+1:raise ValueError('Image does not implement this paired service schema/role')
    for info,(_,data) in zip(metadata,images):info['elf_sha256']=data[176:208].hex()
    total=HEADER+sum(len(data) for board,data in images);header=bytearray(HEADER)
    struct.pack_into('<16sIIII32s',header,0,MAGIC,1,2,total,0,version.encode())
    offset=HEADER
    for i,(board,data) in enumerate(images):
        struct.pack_into('<24s32sII32s',header,64+128*i,board.encode(),version.encode(),len(data),offset,hashlib.sha256(data).digest());offset+=len(data)
    struct.pack_into('<I',header,508,zlib.crc32(header[:508]))
    payload=bytes(header)+b''.join(data for board,data in images)
    return payload,{'format':1,'system_version':version,'bytes':len(payload),'sha256':hashlib.sha256(payload).hexdigest(),'images':metadata,'installed':False,'hardware_validated':False,'minimum_paired_updater':'0.5.0','from_0_4_requires_two_board_bootstrap':True}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--s3',type=Path,required=True);p.add_argument('--c5',type=Path,required=True);p.add_argument('--version',default='0.5.0');p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    data,meta=package(a.s3.read_bytes(),a.c5.read_bytes(),a.version);a.output.mkdir(parents=True,exist_ok=True)
    name=f'CYD-OS-{a.version}-paired.cydos';(a.output/name).write_bytes(data);(a.output/(name+'.json')).write_text(json.dumps(meta,indent=2)+'\n');print(name,len(data),meta['sha256'])
if __name__=='__main__':main()
