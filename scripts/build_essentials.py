#!/usr/bin/env python3
"""Build six offline .cydapp packages for both CYD-OS boards."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import zipfile
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('cydctl',ROOT/'tools/cydctl/cydctl.py')
cydctl=importlib.util.module_from_spec(spec);spec.loader.exec_module(cydctl)
APPS={'calculator':('Calculator',[]),'calendar':('Calendar',['storage','documents']),
      'clock':('Clock',['storage']),'notes':('Notes',['storage','documents']),
      'contacts':('Contacts',['storage','documents']),'files':('Files',['documents'])}
def build(output,staging=None):
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    staging=Path(staging or ROOT/'build-essentials');staging.mkdir(parents=True,exist_ok=True)
    source=ROOT/'apps/essentials';common=(source/'common.lua').read_text()
    results=[]
    for key,(name,permissions) in APPS.items():
        folder=staging/key;folder.mkdir(exist_ok=True)
        manifest={'package_id':'com.cydos.'+key,'name':name,'version':'1.0.0','api_version':1,
                  'entry_point':'main.lua','target':['esp32c5','esp32s3'],'permissions':permissions}
        code=(source/(key+'.lua')).read_text()
        (folder/'main.lua').write_text((common+'\n' if key!='calculator' else '')+code)
        (folder/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        _,data=cydctl.package_project(folder)
        inspected=cydctl.inspect_package(data)
        assert inspected['package_id']==manifest['package_id']
        filename=name+'-1.0.0.cydapp';(output/filename).write_bytes(data)
        results.append({'app':name,'package_id':manifest['package_id'],'file':filename,
                        'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'permissions':permissions})
    (output/'apps.json').write_text(json.dumps(results,indent=2)+'\n')
    with zipfile.ZipFile(output/'Offline-Essentials-Apps.zip','w',zipfile.ZIP_DEFLATED) as z:
        for item in results:z.write(output/item['file'],item['file'])
    return results
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'dist/essentials');p.add_argument('--staging',type=Path)
    a=p.parse_args()
    for item in build(a.output,a.staging):print(item['file'],item['bytes'],item['sha256'])
if __name__=='__main__':main()
