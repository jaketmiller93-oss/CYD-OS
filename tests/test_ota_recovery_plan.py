"""Offline recovery planner never overwrites the confirmed metadata sector."""
from pathlib import Path
import importlib.util, struct, unittest
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('recovery_plan',ROOT/'scripts/ota_recovery_plan.py')
planner = importlib.util.module_from_spec(spec); spec.loader.exec_module(planner)
def data(entries):
    raw = bytearray(b'\xff'*8192)
    for i,(seq,state) in enumerate(entries):
        struct.pack_into('<I',raw,i*4096,seq)
        struct.pack_into('<II',raw,i*4096+24,state,planner.sequence_crc(seq))
    return bytes(raw)
class RecoveryPlan(unittest.TestCase):
    def test_select_other_slot_as_new_preserving_confirmed_sector(self):
        for entries in [[(5,2),(4,2)],[(4,2),(5,2)],[(6,2),(5,2)],[(5,2),(6,2)]]:
            raw = data(entries); plan,page = planner.plan_selection(raw)
            self.assertEqual(plan['target_slot'],1-plan['current_slot'])
            self.assertNotEqual(plan['selection_sector_address'],plan['preserved_selection_sector_address'])
            self.assertEqual(len(page),4096)
            self.assertEqual(struct.unpack_from('<I',page,24)[0],0)
            seq = struct.unpack_from('<I',page)[0]
            self.assertEqual(struct.unpack_from('<I',page,28)[0],planner.sequence_crc(seq))
            self.assertEqual((seq-1)%2,plan['target_slot'])
    def test_pending_new_or_undefined_active_state_rejected(self):
        for state in [0,1,0xFFFFFFFF]:
            with self.assertRaises(ValueError):planner.plan_selection(data([(4,2),(5,state)]))
    def test_invalid_or_aborted_selection_excluded(self):
        for state in [3,4]:
            plan,_=planner.plan_selection(data([(5,2),(6,state)]))
            self.assertEqual(plan['current_slot'],0)
    def test_corrupt_crc_cannot_select_active_image(self):
        raw = bytearray(data([(5,2),(6,2)]));raw[4096+28]^=1
        plan,_=planner.plan_selection(raw);self.assertEqual(plan['current_slot'],0)
    def test_missing_or_short_ota_state_rejected(self):
        for raw in [b'',b'\xff'*4096,b'\xff'*8192]:
            with self.assertRaises(ValueError):planner.plan_selection(raw)
    def test_ambiguous_sequence_rejected(self):
        with self.assertRaises(ValueError):planner.plan_selection(data([(5,2),(5,2)]))
    def test_exhausted_sequence_rejected(self):
        with self.assertRaises(ValueError):planner.plan_selection(data([(0xFFFFFFFE,2)]))
