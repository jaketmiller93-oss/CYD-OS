"""Runs actual Lua model, CYDOS loader and Lua/LVGL UI. No source-string assertions."""
import ctypes as C
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT=Path(__file__).resolve().parents[1]
OS=Path(os.environ.get('CYDOS_SOURCE','../CYD-OS')).resolve()
BUILD=ROOT/'build-host'

def run(*args,**kw):
    return subprocess.run(args,check=True,capture_output=True,text=True,**kw)

class Model(unittest.TestCase):
    def check_lua(self,code):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'test.lua'
            path.write_text((ROOT/'src/model.lua').read_text()+'\n'+code)
            result=run(str(BUILD/'model_host'),str(path))
            self.assertEqual(result.returncode,0)
    def test_defaults_and_roundtrip_format(self):
        self.check_lua('local s=Pet.new();s.checksum=Pet.checksum(s);assert(Pet.valid(s));assert(#Pet.encode(s)<1024);assert(s.food==80 and s.health==100)')
    def test_feed_tradeoffs_and_full_guard(self):
        self.check_lua('local s=Pet.new();assert(Pet.act(s,"feed"));assert(s.food==100 and s.clean==82 and s.energy==89 and s.care==1);assert(not Pet.act(s,"feed"))')
    def test_clean_guard(self):
        self.check_lua('local s=Pet.new();s.clean=10;assert(Pet.act(s,"wash"));assert(s.clean==45);s.clean=100;assert(not Pet.act(s,"wash"))')
    def test_sleep_and_wake(self):
        self.check_lua('local s=Pet.new();s.energy=30;assert(Pet.act(s,"sleep"));Pet.advance(s,60);assert(s.energy==36 and s.food==79);assert(not Pet.act(s,"feed"));assert(not Pet.act(s,"play"));assert(Pet.act(s,"sleep"));assert(not s.sleeping)')
    def test_game_rewards_and_requirements(self):
        self.check_lua('local s=Pet.new();s.energy=10;assert(not Pet.act(s,"play"));s.energy=50;s.food=10;assert(not Pet.act(s,"play"));s.food=50;assert(Pet.act(s,"play"));local joy=s.joy;for i=1,3 do assert(Pet.act(s,"star"))end;assert(s.joy==joy+18 and s.energy==44 and s.food==47)')
    def test_decay_and_fractional_minute(self):
        self.check_lua('local s=Pet.new();Pet.advance(s,59);assert(s.food==80 and s.remainder==59);Pet.advance(s,1);assert(s.food==78 and s.joy==73 and s.clean==84 and s.energy==88 and s.age==60)')
    def test_coalesced_tick_catchup_bound(self):
        self.check_lua('local s=Pet.new();Pet.advance(s,999999999);assert(s.age==300 and s.food==70);Pet.advance(s,-20);assert(s.age==300)')
    def test_auto_sleep_at_zero_energy(self):
        self.check_lua('local s=Pet.new();s.energy=1;Pet.advance(s,60);assert(s.sleeping and s.energy==0);Pet.advance(s,60);assert(s.energy==6)')
    def test_health_recovery_no_death(self):
        self.check_lua('local s=Pet.new();s.food=0;s.joy=0;s.health=2;Pet.advance(s,60);assert(s.health==0);s.food=100;s.joy=100;s.clean=100;s.energy=100;Pet.advance(s,60);assert(s.health==1)')
    def test_moods_and_growth(self):
        self.check_lua('local s=Pet.new();assert(Pet.mood(s)=="happy" and Pet.stage(s)=="Baby");s.food=10;assert(Pet.mood(s)=="hungry");s.food=80;s.joy=10;assert(Pet.mood(s)=="bored");s.joy=80;s.clean=10;assert(Pet.mood(s)=="dirty");s.health=10;assert(Pet.mood(s)=="sick");s.sleeping=true;assert(Pet.mood(s)=="sleep");s.age=1800;assert(Pet.stage(s)=="Buddy");s.age=7200;assert(Pet.stage(s)=="Grown-up")')
    def test_names_and_json_safety(self):
        self.check_lua('assert(Pet.name("  Pip  ")=="Pip");for _,n in ipairs({"", " ", "abcdefghijklmnop", "quote\\\"", "a\\nb", "a/b", "a\\0b"})do assert(Pet.name(n)==nil)end;assert(Pet.name("Pal-2")=="Pal-2")')
    def test_journal_newest_and_corruption_fallback(self):
        self.check_lua('local a,b=Pet.new(),Pet.new();a.sequence=5;b.sequence=7;a.checksum=Pet.checksum(a);b.checksum=Pet.checksum(b);assert(Pet.restore(a,b)==b);b.food=0;assert(Pet.restore(a,b)==a);a.checksum=0;local s,reason=Pet.restore(a,b);assert(reason=="recovered" and s.name=="Mochi")')
    def test_malformed_saves(self):
        self.check_lua('for _,bad in ipairs({false,12,"bad",{}, {version=1}})do assert(not Pet.valid(bad))end;for _,key in ipairs({"food","joy","clean","energy","health","age","remainder","sequence","care","name"})do local s=Pet.new();s.checksum=Pet.checksum(s);s[key]=nil;assert(not Pet.valid(s))end;local s=Pet.new();s.food=0/0;assert(not Pet.valid(s));s.food=101;assert(not Pet.valid(s));s.food=-1;assert(not Pet.valid(s))')
    def test_saturation_and_unknown_action(self):
        self.check_lua('local s=Pet.new();s.age=1000000000;s.care=1000000000;Pet.advance(s,300);assert(s.age==1000000000);assert(Pet.act(s,"feed"));assert(s.care==1000000000);assert(not Pet.act(s,"bogus"))')

class Packaging(unittest.TestCase):
    def test_limits_permissions_and_sprite_integrity(self):
        p=ROOT/'dist/pocket-pal-0.1.0.cydapp'
        with zipfile.ZipFile(p) as z:
            self.assertIsNone(z.testzip());self.assertLessEqual(p.stat().st_size,512*1024)
            self.assertLessEqual(len(z.namelist()),48)
            for info in z.infolist():
                self.assertEqual(info.compress_type,zipfile.ZIP_STORED)
                self.assertLessEqual(info.file_size,65536)
                if info.filename.endswith('.rgb565'):
                    raw=z.read(info);self.assertEqual(raw[:4],b'CIMG');self.assertEqual(len(raw),8+80*64*2)
            manifest=json.loads(z.read('manifest.json'));self.assertEqual(manifest['permissions'],['storage'])
            self.assertEqual(manifest['target'],['esp32c5','esp32s3'])
    def test_actual_firmware_package_preflight_c5_and_s3(self):
        with tempfile.TemporaryDirectory() as d:
            cj=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
            for target in ['esp32c5','esp32s3']:
                out=Path(d)/(target+'.so')
                run('cc','-shared','-fPIC','-Wall','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-DCYD_HOST_TEST',f'-DCONFIG_IDF_TARGET="{target}"','-I'+str(OS/'tests/host'),'-I'+str(OS/'tests'),'-I'+str(cj),str(OS/'tests/package_preview_harness.c'),str(OS/'components/cyd_common/zip.c'),str(OS/'components/cyd_common/json.c'),str(OS/'components/app_manager/manifest.c'),str(OS/'components/app_manager/package.c'),str(cj/'cJSON.c'),'-lm','-o',str(out))
                lib=C.CDLL(str(out));lib.test_package_preview.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p,C.c_size_t]
                raw=(ROOT/'dist/pocket-pal-0.1.0.cydapp').read_bytes();buffer=C.create_string_buffer(8192)
                self.assertEqual(lib.test_package_preview(raw,len(raw),buffer,len(buffer)),0,buffer.value)
                self.assertEqual(json.loads(buffer.value)['package_id'],'org.cyd.pocketpal')

class UI(unittest.TestCase):
    def exercise(self,width,height,name):
        shutil.rmtree('/tmp/pocket-pal-host-data',ignore_errors=True)
        result=run(str(BUILD/'pet_host'),str(ROOT/'app'),str(width),str(height),str(ROOT/'dist'/name))
        print(result.stdout.strip())
        shutil.rmtree('/tmp/pocket-pal-host-data',ignore_errors=True)
    def test_c5_landscape(self):self.exercise(320,240,'preview-c5.ppm')
    def test_c5_portrait(self):self.exercise(240,320,'preview-portrait.ppm')
    def test_s3(self):self.exercise(800,480,'preview-s3.ppm')

if __name__=='__main__':unittest.main()
