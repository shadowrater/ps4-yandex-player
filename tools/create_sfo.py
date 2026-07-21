#!/usr/bin/env python3
"""
Create param.sfo for OpenOrbis PS4 apps.
This generates the metadata file needed for PS4 to recognize the app.
"""
import struct
import sys
import os

def create_param_sfo(title_id, title_name, app_version, output_path):
    """
    Create a minimal param.sfo file for PS4 homebrew.
    
    Format: SFO header + key table + value table + index table
    """
    
    # SFO magic
    magic = b'\x00PSF'
    version = b'\x01\x01\x00\x00'
    
    # Keys we need
    keys = [
        b"APP_VER\x00",
        b"ATTRIBUTE\x00",
        b"CATEGORY\x00",
        b"CONTENT_ID\x00",
        b"DISC_ID\x00",
        b"GITHUB_REPO\x00",
        b"INTENT\x00",
        b"PARENTAL_LEVEL\x00",
        b"TITLE\x00",
        b"TITLE_ID\x00",
        b"VERSION\x00",
    ]
    
    # Values
    values = {
        b"APP_VER\x00": app_version.encode('ascii') + b'\x00',
        b"ATTRIBUTE\x00": struct.pack('<I', 0),
        b"CATEGORY\x00": b'g\x00',
        b"CONTENT_ID\x00": (title_id + '_00-XXXXX1234567_00-XXXXXXXXXXXXXXXX00').encode('ascii')[:63] + b'\x00',
        b"DISC_ID\x00": (title_id + '-P0000-XXYY').encode('ascii')[:63] + b'\x00',
        b"GITHUB_REPO\x00": b'',
        b"INTENT\x00": struct.pack('<I', 0),
        b"PARENTAL_LEVEL\x00": struct.pack('<I', 1),
        b"TITLE\x00": title_name.encode('utf-8') + b'\x00',
        b"TITLE_ID\x00": title_id.encode('ascii') + b'\x00',
        b"VERSION\x00": b'01.00\x00',
    }
    
    # Build key table
    key_table = b''
    key_offsets = {}
    offset = 0
    for key in keys:
        key_offsets[key] = offset
        key_table += key
        offset += len(key)
    
    # Pad key table to 4-byte alignment
    while len(key_table) % 4 != 0:
        key_table += b'\x00'
    
    # Build value table
    value_table = b''
    value_offsets = {}
    offset = 0
    for key in keys:
        val = values.get(key, b'\x00')
        value_offsets[key] = offset
        value_table += val
        offset += len(val)
    
    # Pad value table to 4-byte alignment
    while len(value_table) % 4 != 0:
        value_table += b'\x00'
    
    # Build index table (one entry per key)
    index_table = b''
    for key in keys:
        key_off = key_offsets[key]
        val_off = value_offsets[key]
        val_fmt = 0x0204 if isinstance(values.get(key, b''), bytes) and len(values.get(key, b'')) > 4 else 0x0204
        
        # Index entry: key_offset(2), padding(1), format(1), length(4), max_length(4), data_offset(4)
        index_table += struct.pack('<HBB III',
            key_off,      # key offset
            0,            # padding
            0x04,         # format (utf-8)
            len(values.get(key, b'\x00')),  # length
            len(values.get(key, b'\x00')),  # max length
            val_off       # data offset
        )
    
    # Calculate offsets
    header_size = 20  # magic(4) + version(4) + keytable_off(4) + valtable_off(4) + indextable_off(4)
    key_table_off = header_size
    val_table_off = key_table_off + len(key_table)
    index_table_off = val_table_off + len(value_table)
    total_size = index_table_off + len(index_table)
    
    # Build SFO
    sfo = b''
    sfo += magic
    sfo += version
    sfo += struct.pack('<IIII',
        total_size,           # total file size
        key_table_off,        # key table offset
        val_table_off,        # value table offset
        index_table_off       # index table offset
    )
    sfo += key_table
    sfo += value_table
    sfo += index_table
    
    # Write file
    os.makedirs(os.path.dirname(output_path) or '.', exist_ok=True)
    with open(output_path, 'wb') as f:
        f.write(sfo)
    
    print(f"[param.sfo] Created: {output_path} ({len(sfo)} bytes)")
    return True


if __name__ == '__main__':
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <title_id> <title_name> <app_version> [output_path]")
        print(f"Example: {sys.argv[0]} DEAD0001 'Yandex Music Player' 01.00 build/sce_sys/param.sfo")
        sys.exit(1)
    
    title_id = sys.argv[1]
    title_name = sys.argv[2]
    app_version = sys.argv[3]
    output = sys.argv[4] if len(sys.argv) > 4 else 'build/sce_sys/param.sfo'
    
    create_param_sfo(title_id, title_name, app_version, output)
