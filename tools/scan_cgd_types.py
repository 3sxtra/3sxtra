#!/usr/bin/env python3
"""
Scan the decrypted CPS3 ROM for cgd_type values in all character script tables.

This script mimics the read_char_table() parser from arcade_char_data.c to check
whether any character animation script uses cgd_type == 1.

If cgd_type == 1 exists, then Artem's SDL_max(cgd_type * 4 - 8, 0) fix is
confirmed to corrupt the data extraction by preventing the necessary -4 rewind.

Usage:
    python scan_cgd_types.py <path_to_decrypted_rom_binary>

The decrypted ROM can be obtained by running the game with dump code, or by
using the rom_load.c decrypt() function externally.
"""

import struct
import sys
import os

BASE_OFFSET = 0x6000000

# Character names for readable output
CHAR_NAMES = [
    "Alex", "Yun", "Dudley", "Necro", "Hugo", "Ibuki", "Elena", "Oro",
    "Yang", "Ken", "Sean", "Urien", "Gill", "Chun-Li", "Makoto", "Akuma",
    "Twelve", "Remy", "Q", "Chunli-Alt"
]

# Script table names (the ones that use read_char_table)
SCRIPT_TABLES = ["nmca", "dmca", "btca", "caca", "cuca", "atca", "saca", "exca", "cbca", "yuca"]

# LocationData for all 20 characters - offset and size for each script table
# Extracted directly from the PR's location_data array in arcade_char_data.c
LOCATION_DATA = [
    # Character 0 - Alex
    {"nmca": (0x1CB230, 0x2660), "dmca": (0x1CD890, 0x25B4), "btca": (0x1CFE44, 0x1554),
     "caca": (0x1D1398, 0x578), "cuca": (0x1D1910, 0x2D54), "atca": (0x1D4664, 0x2358),
     "saca": (0x1DBC98, 0x25B4), "exca": (0x1D9ECC, 0x1DCC), "cbca": (0x1DE24C, 0x2F0),
     "yuca": (0x36068C, 0x1388)},
    # Character 1 - Yun
    {"nmca": (0x1DE53C, 0x1C58), "dmca": (0x1E0194, 0x27EC), "btca": (0x1E2980, 0x1694),
     "caca": (0x1E4014, 0x33E8), "cuca": (0x1E73FC, 0x2D1C), "atca": (0x1EA118, 0x2994),
     "saca": (0x1F4EC4, 0x3EE8), "exca": (0x1F33A4, 0x1B20), "cbca": (0x1F8DAC, 0x4BC),
     "yuca": (0x361A14, 0x1150)},
    # Character 2 - Dudley
    {"nmca": (0x1F9268, 0x16D0), "dmca": (0x1FA938, 0x217C), "btca": (0x1FCAB4, 0x1584),
     "caca": (0x1FE038, 0x724), "cuca": (0x1FE75C, 0x2D24), "atca": (0x201480, 0x2114),
     "saca": (0x20669C, 0x2748), "exca": (0x20476C, 0x1F30), "cbca": (0x208DE4, 0x9D0),
     "yuca": (0x362B64, 0x6E8)},
    # Character 3 - Necro
    {"nmca": (0x2097B4, 0x22A4), "dmca": (0x20BA58, 0x275C), "btca": (0x20E1B4, 0x14EC),
     "caca": (0x20F6A0, 0x184C), "cuca": (0x210EEC, 0x2D14), "atca": (0x213C00, 0x6AC4),
     "saca": (0x2218E0, 0x7D40), "exca": (0x21E604, 0x32DC), "cbca": (0x229620, 0xE34),
     "yuca": (0x36324C, 0x15F8)},
    # Character 4 - Hugo
    {"nmca": (0x22A454, 0x1C0C), "dmca": (0x22C060, 0x2954), "btca": (0x22E9B4, 0x160C),
     "caca": (0x22FFC0, 0x7B8), "cuca": (0x230778, 0x2CA4), "atca": (0x23341C, 0x4200),
     "saca": (0x23A644, 0x6AE4), "exca": (0x238DC4, 0x1880), "cbca": (0x241128, 0xF10),
     "yuca": (0x364844, 0xD58)},
    # Character 5 - Ibuki
    {"nmca": (0x242038, 0x1D78), "dmca": (0x243DB0, 0x278C), "btca": (0x24653C, 0x132C),
     "caca": (0x247868, 0x2124), "cuca": (0x24998C, 0x2D04), "atca": (0x24C690, 0x3A04),
     "saca": (0x254AE4, 0x3FF8), "exca": (0x2536C4, 0x1420), "cbca": (0x258ADC, 0x534),
     "yuca": (0x36559C, 0xE20)},
    # Character 6 - Elena
    {"nmca": (0x259010, 0x1A48), "dmca": (0x25AA58, 0x241C), "btca": (0x25CE74, 0x156C),
     "caca": (0x25E3E0, 0x2C40), "cuca": (0x261020, 0x2D44), "atca": (0x263D64, 0x211C),
     "saca": (0x26ECC0, 0x4164), "exca": (0x26C360, 0x2960), "cbca": (0x272E24, 0x920),
     "yuca": (0x3663BC, 0xD88)},
    # Character 7 - Oro
    {"nmca": (0x273744, 0x2458), "dmca": (0x275B9C, 0x302C), "btca": (0x278BC8, 0x1674),
     "caca": (0x27A23C, 0x2144), "cuca": (0x27C380, 0x2CC4), "atca": (0x27F044, 0x54C4),
     "saca": (0x296D50, 0x9158), "exca": (0x293C18, 0x3138), "cbca": (0x29FEA8, 0xCF4),
     "yuca": (0x367144, 0x1820)},
    # Character 8 - Yang
    {"nmca": (0x2A0B9C, 0x2BB0), "dmca": (0x2A374C, 0x2754), "btca": (0x2A5EA0, 0x148C),
     "caca": (0x2A732C, 0x1DC), "cuca": (0x2A7508, 0x2D8C), "atca": (0x2AA294, 0x45EC),
     "saca": (0x2B2F04, 0x6638), "exca": (0x2AF948, 0x35BC), "cbca": (0x2B953C, 0xA80),
     "yuca": (0x368964, 0x2DA8)},
    # Character 9 - Ken
    {"nmca": (0x2B9FBC, 0x1D88), "dmca": (0x2BBD44, 0x22CC), "btca": (0x2BE010, 0x1474),
     "caca": (0x2BF484, 0x24D0), "cuca": (0x2C1954, 0x2D0C), "atca": (0x2C4660, 0x3540),
     "saca": (0x2CD3F8, 0x4054), "exca": (0x2CB658, 0x1DA0), "cbca": (0x2D144C, 0xB30),
     "yuca": (0x36B70C, 0x1880)},
    # Character 10 - Sean
    {"nmca": (0x2D1F7C, 0x1DEC), "dmca": (0x2D3D68, 0x274C), "btca": (0x2D64B4, 0x150C),
     "caca": (0x2D79C0, 0x161C), "cuca": (0x2D8FDC, 0x2D14), "atca": (0x2DBCF0, 0x603C),
     "saca": (0x2E8F64, 0x7054), "exca": (0x2E5ECC, 0x3098), "cbca": (0x2EFFB8, 0xF50),
     "yuca": (0x36CF8C, 0x11E8)},
    # Character 11 - Urien
    {"nmca": (0x2F0F08, 0x16D0), "dmca": (0x2F25D8, 0x217C), "btca": (0x2F4754, 0x1584),
     "caca": (0x2F5CD8, 0xE38), "cuca": (0x2F6B10, 0x2D24), "atca": (0x2F9834, 0x2CEC),
     "saca": (0x3001EC, 0x26A8), "exca": (0x2FEA40, 0x17AC), "cbca": (0x302894, 0x5D0),
     "yuca": (0x36E174, 0x938)},
    # Character 12 - Gill
    {"nmca": (0x302E64, 0x16D0), "dmca": (0x304534, 0x215C), "btca": (0x306690, 0x1584),
     "caca": (0x307C14, 0x12C0), "cuca": (0x308ED4, 0x2D24), "atca": (0x30BBF8, 0x21AC),
     "saca": (0x311BD0, 0x2A88), "exca": (0x3101BC, 0x1A14), "cbca": (0x314658, 0x73C),
     "yuca": (0x36EAAC, 0xDA8)},
    # Character 13 - Chun-Li
    {"nmca": (0x314D94, 0x1A78), "dmca": (0x31680C, 0x2594), "btca": (0x318DA0, 0x148C),
     "caca": (0x31A22C, 0x568), "cuca": (0x31A794, 0x2D54), "atca": (0x31D4E8, 0x290C),
     "saca": (0x324858, 0x3A54), "exca": (0x32305C, 0x17FC), "cbca": (0x3282AC, 0x47C),
     "yuca": (0x36F854, 0x1058)},
    # Character 14 - Makoto
    {"nmca": (0x328728, 0x16A0), "dmca": (0x329DC8, 0x217C), "btca": (0x32BF44, 0x1584),
     "caca": (0x32D4C8, 0xF3C), "cuca": (0x32E404, 0x2D24), "atca": (0x331128, 0x2810),
     "saca": (0x337EBC, 0x3544), "exca": (0x335978, 0x2544), "cbca": (0x33B400, 0x788),
     "yuca": (0x3708AC, 0xD84)},
    # Character 15 - Akuma
    {"nmca": (0x3723B4, 0x21F8), "dmca": (0x3745AC, 0x2574), "btca": (0x376B20, 0x151C),
     "caca": (0x37803C, 0x684), "cuca": (0x3786C0, 0x2CF4), "atca": (0x37B3B4, 0x3710),
     "saca": (0x382940, 0x57CC), "exca": (0x38024C, 0x26F4), "cbca": (0x38810C, 0x660),
     "yuca": (0x3E2E00, 0x1000)},
    # Character 16 - Twelve
    {"nmca": (0x38876C, 0x2350), "dmca": (0x38AABC, 0x230C), "btca": (0x38CDC8, 0x152C),
     "caca": (0x38E2F4, 0xA8C), "cuca": (0x38ED80, 0x2D04), "atca": (0x391A84, 0x31BC),
     "saca": (0x39D7DC, 0x4E3C), "exca": (0x399898, 0x3F44), "cbca": (0x3A2618, 0x594),
     "yuca": (0x3E3E00, 0x1260)},
    # Character 17 - Remy
    {"nmca": (0x3A2BAC, 0x2358), "dmca": (0x3A4F04, 0x293C), "btca": (0x3A7840, 0x148C),
     "caca": (0x3A8CCC, 0x13A8), "cuca": (0x3AA074, 0x2DAC), "atca": (0x3ACE20, 0x2F5C),
     "saca": (0x3B41D0, 0x52DC), "exca": (0x3B262C, 0x1BA4), "cbca": (0x3B94AC, 0x5B8),
     "yuca": (0x3E5060, 0x13B0)},
    # Character 18 - Q
    {"nmca": (0x3B9A64, 0x1E38), "dmca": (0x3BB89C, 0x297C), "btca": (0x3BE218, 0x134C),
     "caca": (0x3BF564, 0xDF8), "cuca": (0x3C035C, 0x2CD4), "atca": (0x3C3030, 0x2F94),
     "saca": (0x3CA60C, 0x5E50), "exca": (0x3C8AFC, 0x1B10), "cbca": (0x3D045C, 0x89C),
     "yuca": (0x3E6410, 0x11A8)},
    # Character 19 - Chunli-Alt (Gill-Alt)
    {"nmca": (0x3D0CF8, 0x23A0), "dmca": (0x3D3098, 0x2504), "btca": (0x3D559C, 0x1394),
     "caca": (0x3D6930, 0x3E8), "cuca": (0x3D6D18, 0x2D64), "atca": (0x3D9A7C, 0x227C),
     "saca": (0x3DF2B4, 0x36B4), "exca": (0x3DCB10, 0x27A4), "cbca": (0x3E2968, 0x498),
     "yuca": (0x3E75B8, 0x73E0)},
]


def scan_cgd_types(rom_data: bytes):
    """Scan all character script tables for cgd_type values."""
    
    type_counts = {1: 0, 2: 0, 4: 0, 6: 0}
    type_1_locations = []
    
    for char_idx, char_locs in enumerate(LOCATION_DATA):
        char_name = CHAR_NAMES[char_idx] if char_idx < len(CHAR_NAMES) else f"Char_{char_idx}"
        
        for table_name in SCRIPT_TABLES:
            if table_name not in char_locs:
                continue
            
            offset, size = char_locs[table_name]
            
            if offset + size > len(rom_data):
                print(f"  WARNING: {char_name}/{table_name} offset 0x{offset:X}+0x{size:X} exceeds ROM size")
                continue
            
            # Read script offset table (array of uint32 BE, terminated by 0)
            pos = offset
            script_offsets = []
            while True:
                if pos + 4 > offset + size:
                    break
                value = struct.unpack_from('>I', rom_data, pos)[0]
                if value == 0:
                    break
                # Convert absolute ROM pointer to relative offset within this table
                rel_offset = value - BASE_OFFSET - offset
                script_offsets.append(rel_offset)
                pos += 4
            
            if not script_offsets:
                continue
            
            # Sort offsets to process in order
            sorted_offsets = sorted(script_offsets)
            
            # For each script, read the cgd_type header
            for script_offset in sorted_offsets:
                # The header starts 8 bytes before the script pointer
                header_pos = offset + script_offset - 8
                
                if header_pos < 0 or header_pos + 2 > len(rom_data):
                    continue
                
                cgd_type = struct.unpack_from('>h', rom_data, header_pos)[0]
                
                if cgd_type in type_counts:
                    type_counts[cgd_type] += 1
                else:
                    # Unexpected value
                    print(f"  UNEXPECTED cgd_type={cgd_type} at {char_name}/{table_name} script_offset=0x{script_offset:X}")
                
                if cgd_type == 1:
                    type_1_locations.append((char_name, table_name, script_offset))
    
    # Print results
    print("=" * 60)
    print("cgd_type Distribution Across All Character Script Tables")
    print("=" * 60)
    print(f"  cgd_type == 1:  {type_counts[1]:5d} scripts")
    print(f"  cgd_type == 2:  {type_counts[2]:5d} scripts")
    print(f"  cgd_type == 4:  {type_counts[4]:5d} scripts")
    print(f"  cgd_type == 6:  {type_counts[6]:5d} scripts")
    print(f"  Total:          {sum(type_counts.values()):5d} scripts")
    print()
    
    if type_1_locations:
        print(f"*** CONFIRMED: {len(type_1_locations)} scripts use cgd_type == 1 ***")
        print(f"*** SDL_max(cgd_type * 4 - 8, 0) WILL corrupt these scripts! ***")
        print()
        print("Affected locations:")
        for char_name, table_name, script_offset in type_1_locations:
            print(f"  {char_name:12s} / {table_name} @ script_offset 0x{script_offset:X}")
    else:
        print("No scripts use cgd_type == 1.")
        print("SDL_max change is a no-op (harmless).")
    
    return type_counts, type_1_locations


def _rotate_left_16(value, n):
    """Rotate a 16-bit value left by n bits."""
    value &= 0xFFFF
    aux = value >> (16 - n)
    return ((value << n) | aux) % 0x10000


def _rotxor(val, xorval):
    """CPS3 rotate-xor operation."""
    val &= 0xFFFF
    xorval &= 0xFFFF
    res = (val + _rotate_left_16(val, 2)) & 0xFFFFFFFF
    res = _rotate_left_16(res, 4) ^ (res & (val ^ xorval))
    return res


def _cps3_mask(address, key1, key2):
    """Generate CPS3 XOR mask for a given address."""
    address ^= key1
    address &= 0xFFFFFFFF
    val = (address & 0xFFFF) ^ 0xFFFF
    val = _rotxor(val, key2 & 0xFFFF)
    val ^= (address >> 16) ^ 0xFFFF
    val = _rotxor(val, key2 >> 16)
    val ^= (address & 0xFFFF) ^ (key2 & 0xFFFF)
    return (val | (val << 16)) & 0xFFFFFFFF


BASE_OFFSET_DECRYPT = 0x6000000
KEY_1 = 0xA55432B4
KEY_2 = 0x0C129981


def decrypt_simms(zip_path: str) -> bytes:
    """
    Decrypt the CPS3 ROM from a zip containing 4 SIMM files.
    This is a Python port of rom_load.c decrypt() + cps3_decrypt.c.
    """
    import zipfile
    
    simm_names = [
        "sfiii3-simm1.0",
        "sfiii3-simm1.1", 
        "sfiii3-simm1.2",
        "sfiii3-simm1.3",
    ]
    
    with zipfile.ZipFile(zip_path, 'r') as z:
        simms = []
        for name in simm_names:
            data = z.read(name)
            simms.append(data)
            print(f"  Loaded {name}: {len(data)} bytes")
    
    simm_size = len(simms[0])
    assert all(len(s) == simm_size for s in simms), "SIMM sizes don't match!"
    
    print(f"  Decrypting {simm_size} words ({simm_size * 4} bytes)...")
    
    result = bytearray(simm_size * 4)
    
    for i in range(simm_size):
        b0 = simms[0][i]
        b1 = simms[1][i]
        b2 = simms[2][i]
        b3 = simms[3][i]
        
        cur_data = (b0 << 24) | (b1 << 16) | (b2 << 8) | b3
        masked = _cps3_mask(BASE_OFFSET_DECRYPT + (i * 4), KEY_1, KEY_2)
        decrypted = cur_data ^ masked
        
        # Store as big-endian (the C code uses SDL_ReadU32BE/SDL_ReadS16BE to read)
        struct.pack_into('>I', result, i * 4, decrypted)
    
    return bytes(result)


def main():
    # Try to find the ROM zip
    if len(sys.argv) >= 2:
        rom_path = sys.argv[1]
        if rom_path.endswith('.zip'):
            print(f"Loading ROM from zip: {rom_path}")
            rom_data = decrypt_simms(rom_path)
        else:
            print(f"Loading pre-decrypted ROM: {rom_path}")
            with open(rom_path, 'rb') as f:
                rom_data = f.read()
    else:
        # Auto-detect from standard resources path
        appdata = os.environ.get('APPDATA', '')
        rom_path = os.path.join(appdata, 'CrowdedStreet', '3SX', 'resources', 'sfiii3nr1.zip')
        if not os.path.exists(rom_path):
            print(f"Error: ROM not found at {rom_path}")
            print("Usage: python scan_cgd_types.py [path_to_sfiii3nr1.zip | path_to_decrypted.bin]")
            sys.exit(1)
        print(f"Auto-detected ROM: {rom_path}")
        rom_data = decrypt_simms(rom_path)
    
    print(f"ROM size: {len(rom_data)} bytes (0x{len(rom_data):X})")
    print()
    
    scan_cgd_types(rom_data)


if __name__ == "__main__":
    main()
