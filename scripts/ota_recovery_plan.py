"""Offline CYD OTA selection plan. Never opens a port or writes a device.

Call only after independently verifying the installed CYD partition table and
active image. Preserve the active metadata sector and select the other app as
NEW, so the existing bootloader performs normal pending verification/rollback.
"""
import binascii
import struct

SECTOR = 4096
OTA_DATA = 0xF000
SLOTS = (0x20000, 0x720000)

def sequence_crc(sequence):
    return binascii.crc32(struct.pack('<I', sequence), 0xFFFFFFFF) & 0xFFFFFFFF

def plan_selection(otadata):
    if len(otadata) != 2 * SECTOR:
        raise ValueError('Expected the verified 8192-byte CYD OTA data partition')
    valid = []
    for index in range(2):
        entry = otadata[index * SECTOR:index * SECTOR + 32]
        seq = struct.unpack_from('<I', entry)[0]
        state, crc = struct.unpack_from('<II', entry, 24)
        if seq not in (0, 0xFFFFFFFF) and state not in (3, 4) and crc == sequence_crc(seq):
            valid.append((seq, index, state))
    if not valid:
        raise ValueError('No valid confirmed OTA selection; inspect recovery state first')
    valid.sort(reverse=True)
    seq, active, state = valid[0]
    if len(valid) == 2 and valid[0][0] == valid[1][0]:
        raise ValueError('Ambiguous OTA sequence')
    if state != 2:
        raise ValueError('Current boot is not marked VALID; stop before writing')
    if seq >= 0xFFFFFFFE:
        raise ValueError('OTA sequence exhausted; manual recovery review required')
    old_slot = (seq - 1) % 2
    new_slot = 1 - old_slot
    next_seq = seq + 1
    page = bytearray(b'\xFF' * SECTOR)
    struct.pack_into('<I', page, 0, next_seq)
    struct.pack_into('<II', page, 24, 0, sequence_crc(next_seq))  # NEW, never preconfirmed
    plan = {'current_slot': old_slot, 'current_app_address': SLOTS[old_slot],
            'target_slot': new_slot, 'target_app_address': SLOTS[new_slot],
            'selection_sector_address': OTA_DATA + (1 - active) * SECTOR,
            'preserved_selection_sector_address': OTA_DATA + active * SECTOR,
            'new_sequence': next_seq, 'new_state': 'NEW',
            'requires_partition_table_and_image_preflight': True}
    return plan, bytes(page)
