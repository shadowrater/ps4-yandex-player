#!/usr/bin/env python3
"""
Create PS4 PKG file for homebrew apps.
This packages the built ELF and metadata into a PKG that can be
installed on a jailbroken PS4 via Debug Settings.
"""
import struct
import os
import sys
import hashlib

class PkgBuilder:
    """
    Minimal PKG builder for PS4 homebrew.
    Based on PS4 PKG format specification.
    """
    
    # PKG magic and constants
    MAGIC = 0x7F434E54  # .CNT
    VERSION = 0x20000
    HDR_SIZE = 0x200
    
    # Item types
    ITEM_ENTRY = 0x0004
    ITEM_SC0   = 0x0001
    ITEM_PFS   = 0x000E
    ITEM_SFO   = 0x000F
    ITEM_ICO   = 0x0010
    ITEM_XML   = 0x0011
    ITEM_ELF   = 0x0012
    ITEM_NPDRM = 0x0013
    ITEM_LICENSE = 0x0014
    ITEM_SHARE = 0x0015
    ITEM_CHUNK = 0x0016
    ITEM_SPLASH = 0x0017
    ITEM_SFX   = 0x0018
    
    def __init__(self, build_dir, output_path, title_id):
        self.build_dir = build_dir
        self.output_path = output_path
        self.title_id = title_id
        self.items = []
        self.data_offset = 0
        self.data_buffer = bytearray()
        
    def add_file(self, filepath, item_type, name=None):
        """Add a file as a PKG item."""
        if not os.path.exists(filepath):
            print(f"[PKG] Warning: {filepath} not found, skipping")
            return False
            
        with open(filepath, 'rb') as f:
            data = f.read()
        
        self.items.append({
            'type': item_type,
            'name': name or os.path.basename(filepath),
            'data': data,
            'offset': self.data_offset,
            'size': len(data),
        })
        
        self.data_offset += len(data)
        return True
    
    def add_raw(self, data, item_type, name):
        """Add raw data as a PKG item."""
        self.items.append({
            'type': item_type,
            'name': name,
            'data': data,
            'offset': self.data_offset,
            'size': len(data),
        })
        
        self.data_offset += len(data)
    
    def build(self):
        """Build the PKG file."""
        print(f"[PKG] Building package for {self.title_id}...")
        
        # 1. Add param.sfo
        sfo_path = os.path.join(self.build_dir, 'sce_sys', 'param.sfo')
        self.add_file(sfo_path, self.ITEM_SFO, 'param.sfo')
        
        # 2. Add ELF binary
        elf_path = os.path.join(self.build_dir, f'{self.title_id}.elf')
        self.add_file(elf_path, self.ITEM_ELF, f'{self.title_id}.elf')
        
        # 3. Add icon (if exists)
        icon_path = os.path.join(self.build_dir, 'sce_sys', 'icon0.png')
        self.add_file(icon_path, self.ITEM_ICO, 'icon0.png')
        
        # 4. Create NPDRM metadata
        npdrm_data = self._create_npdrm()
        self.add_raw(npdrm_data, self.ITEM_NPDRM, 'npdrm')
        
        # 5. Create license data
        license_data = self._create_license()
        self.add_raw(license_data, self.ITEM_LICENSE, 'license.dat')
        
        # 6. Build header
        header = self._build_header()
        
        # 7. Write everything
        os.makedirs(os.path.dirname(self.output_path) or '.', exist_ok=True)
        with open(self.output_path, 'wb') as f:
            f.write(header)
            for item in self.items:
                f.write(item['data'])
        
        total_size = len(header) + self.data_offset
        print(f"[PKG] Created: {self.output_path} ({total_size:,} bytes)")
        print(f"[PKG] Items: {len(self.items)}")
        
        for item in self.items:
            print(f"  - {item['name']}: {item['type']:#06x} ({item['size']:,} bytes)")
        
        return True
    
    def _create_npdrm(self):
        """Create NPDRM metadata for homebrew (no license check)."""
        # NPDRM format (simplified)
        npdrm = bytearray(0x100)
        
        # Magic
        npdrm[0:4] = b'NPD\0'
        
        # Content type: 0 = PS4 App
        struct.pack_into('<I', npdrm, 4, 0)
        
        # Content ID
        content_id = f"{self.title_id}-XX0000-NPXX00000_00-XXXXXXXXXXXXXXXX00"
        npdrm[8:72] = content_id.encode('ascii')[:64].ljust(64, b'\x00')
        
        # DRM type: 0 = No DRM (homebrew)
        struct.pack_into('<I', npdrm, 72, 0)
        
        # Content type
        struct.pack_into('<I', npdrm, 76, 1)
        
        # Package type
        struct.pack_into('<I', npdrm, 80, 6)  # PS4 Disc Package
        
        return bytes(npdrm)
    
    def _create_license(self):
        """Create empty license file."""
        # Minimal license file
        license_data = bytearray(0x400)
        license_data[0:4] = b'LIC\0'
        struct.pack_into('<I', license_data, 4, 0)  # type
        return bytes(license_data)
    
    def _build_header(self):
        """Build PKG header."""
        header = bytearray(self.HDR_SIZE)
        
        # Magic
        struct.pack_into('<I', header, 0, self.MAGIC)
        
        # Version
        struct.pack_into('<I', header, 4, self.VERSION)
        
        # Header size
        struct.pack_into('<I', header, 8, self.HDR_SIZE)
        
        # Item count
        struct.pack_into('<I', header, 12, len(self.items))
        
        # Total content size
        struct.pack_into('<I', header, 16, self.data_offset)
        
        # Item table offset (right after header)
        struct.pack_into('<I', header, 20, self.HDR_SIZE)
        
        # Build item table
        item_entry_size = 0x20  # 32 bytes per item
        item_table = bytearray(len(self.items) * item_entry_size)
        
        for i, item in enumerate(self.items):
            off = i * item_entry_size
            
            # Item type
            struct.pack_into('<I', item_table, off, item['type'])
            
            # Item offset (from start of data, after all items)
            data_start = self.HDR_SIZE + len(self.items) * item_entry_size
            struct.pack_into('<Q', item_table, off + 4, data_start + item['offset'])
            
            # Item size
            struct.pack_into('<Q', item_table, off + 12, item['size'])
            
            # Item index
            struct.pack_into('<I', item_table, off + 20, i)
            
            # Unknown/padding
            struct.pack_into('<I', item_table, off + 24, 0)
            struct.pack_into('<I', item_table, off + 28, 0)
        
        # Append item table
        header.extend(item_table)
        
        return bytes(header)


def main():
    if len(sys.argv) < 4:
        print(f"Usage: {sys.argv[0]} <build_dir> <output.pkg> <title_id>")
        print(f"Example: {sys.argv[0]} build build/DEAD0001.pkg DEAD0001")
        sys.exit(1)
    
    build_dir = sys.argv[1]
    output_path = sys.argv[2]
    title_id = sys.argv[3]
    
    builder = PkgBuilder(build_dir, output_path, title_id)
    builder.build()


if __name__ == '__main__':
    main()
