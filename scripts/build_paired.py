#!/usr/bin/env python3
"""Build/review one paired release, preserving all existing private build configs."""
import argparse,importlib.util,json,os,re,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def module(name,file):
    spec=importlib.util.spec_from_file_location(name,ROOT/'scripts'/file);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
ota=module('paired_build_ota','build_ota.py');paired=module('paired_container','package_paired.py')
# Paired C5 adds always-on manager services. Keep eligible networking data in
# PSRAM; ESP-IDF retains the internal-only radio/DMA buffers and task stacks.
C5_MEMORY_CONFIG={'CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP':'y',
                  'CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL':'0'}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--version',default=(ROOT/'versions/paired.txt').read_text().strip());p.add_argument('--output',type=Path,default=ROOT/'dist/paired');p.add_argument('--s3-toolchain',type=Path,help='Optional Xtensa toolchain bin directory');p.add_argument('--skip-build',action='store_true',help='Package already built, matching credential-free dedicated outputs');a=p.parse_args()
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+(?:[a-zA-Z0-9.-]{0,16})?',a.version) or len(a.version.encode())>=32:p.error('Use one short system version')
    if not a.skip_build and not os.getenv('IDF_PATH'):p.error('Activate ESP-IDF 5.5.4 first')
    a.output.mkdir(parents=True,exist_ok=True);bootstrap=a.output/'Bootstrap';bootstrap.mkdir(exist_ok=True);images={};verification={}
    for board in ['nm_cyd_c5','crowpanel_s3']:
        build=ROOT/f'build-paired-{a.version}-{board}';build.mkdir(exist_ok=True);config=build/'sdkconfig';link=ota.link_config(board,ota.LINK_PROFILE)
        if config.exists():
            text=config.read_text()
            for key in ota.PRIVATE:
                value=re.search(r'^'+key+'=(.*)$',text,re.M)
                if value and value[1]!='""':p.error('Private field in dedicated build; use another version/build directory: '+key)
        defaults=build/'paired.defaults';flags={'CONFIG_CYD_BOARD_CONFIRMED':'y','CONFIG_CYD_PAIRED_SYSTEM':'y',**link,**{key:'""'for key in ota.PRIVATE}}
        if board=='nm_cyd_c5':flags.update(CONFIG_BT_ENABLED='y',CONFIG_BT_BLUEDROID_ENABLED='n',CONFIG_BT_NIMBLE_ENABLED='y',CONFIG_BT_NIMBLE_MAX_CONNECTIONS='1',CONFIG_BT_NIMBLE_MAX_BONDS='4',CONFIG_BT_NIMBLE_NVS_PERSIST='y',CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL='y',CONFIG_BT_NIMBLE_ROLE_PERIPHERAL='n',CONFIG_BT_NIMBLE_ROLE_BROADCASTER='n')
        if board=='nm_cyd_c5':flags.update(C5_MEMORY_CONFIG)
        defaults.write_text(''.join(k+'='+v+'\n'for k,v in flags.items()))
        if not a.skip_build:
            env=os.environ.copy()
            if board=='crowpanel_s3'and a.s3_toolchain:env['PATH']=str(a.s3_toolchain)+os.pathsep+env['PATH']
            target='esp32c5'if board=='nm_cyd_c5'else'esp32s3'
            subprocess.run(['idf.py','-B',str(build),'-D','CYD_BOARD='+board,'-D','IDF_TARGET='+target,'-D','PROJECT_VER='+a.version,'-D','SDKCONFIG='+str(config),'-D','SDKCONFIG_DEFAULTS=sdkconfig.defaults;'+str(defaults),'build'],cwd=ROOT,env=env,check=True)
        text=config.read_text();ota.verify_link_config(text,link)
        for key in ota.PRIVATE:
            if not re.search(r'^'+key+r'=""$',text,re.M):raise ValueError('Initial private field is not blank: '+key)
        if not re.search(r'^CONFIG_CYD_PAIRED_SYSTEM=y$',text,re.M):raise ValueError('Not a paired role build')
        if board=='nm_cyd_c5':
            ota.verify_link_config(text,C5_MEMORY_CONFIG)
            for key in ['CONFIG_BT_ENABLED','CONFIG_BT_NIMBLE_ENABLED','CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL','CONFIG_BT_NIMBLE_NVS_PERSIST']:
                if not re.search(r'^'+key+'=y$',text,re.M):raise ValueError('Missing C5 manager configuration: '+key)
        data=(build/'cyd-os.bin').read_bytes();meta=ota.validate_image(data,board,a.version);meta.update(elf_sha256=data[176:208].hex(),role='device_manager'if board=='nm_cyd_c5'else'application_host',paired_service_schema=1,link_profile=ota.LINK_PROFILE,link_config=link,initial_private_fields_blank=True,installed=False,hardware_validated=False)
        short='C5'if board=='nm_cyd_c5'else'S3';name=f'CYD-OS-{a.version}-{short}-OTA.bin';(bootstrap/name).write_bytes(data);(bootstrap/(name+'.json')).write_text(json.dumps(meta,indent=2)+'\n');images[board]=data;verification[board]=meta
    data,metadata=paired.package(images['crowpanel_s3'],images['nm_cyd_c5'],a.version);name=f'CYD-OS-{a.version}-paired.cydos';(a.output/name).write_bytes(data);(a.output/(name+'.json')).write_text(json.dumps(metadata,indent=2)+'\n');(a.output/'build-verification.json').write_text(json.dumps(verification,indent=2)+'\n');print('Verified paired candidate:',name,metadata['bytes'],metadata['sha256'])
if __name__=='__main__':main()
