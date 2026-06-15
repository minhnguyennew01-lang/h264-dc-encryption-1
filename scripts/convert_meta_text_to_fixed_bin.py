#!/usr/bin/env python3
import sys
import struct
from pathlib import Path

def parse_indices_from_text_meta(path):
    indices = []
    for line in path.read_text().splitlines():
        line = line.strip()
        if line.startswith('NALU '):
            # format: NALU <idx>: Type X @ offset Y
            parts = line.split()
            try:
                idx = int(parts[1].rstrip(':'))
                indices.append(idx)
            except Exception:
                continue
    return indices

def write_binary_meta(path, magic, indices):
    with open(path, 'wb') as f:
        f.write(magic.encode('ascii'))
        for idx in indices:
            f.write(struct.pack('<I', idx))

def main():
    if len(sys.argv) != 3:
        print('Usage: convert_meta_text_to_fixed_bin.py <text.meta> <magic>')
        print('Example: convert_meta_text_to_fixed_bin.py output.h264.s1_hybrid_fixed.meta S1FX')
        sys.exit(1)

    meta_path = Path(sys.argv[1])
    magic = sys.argv[2]
    if not meta_path.exists():
        print('Meta file not found:', meta_path)
        sys.exit(1)

    indices = parse_indices_from_text_meta(meta_path)
    if not indices:
        print('No NALU indices found in text meta; nothing to write')
        sys.exit(1)

    # Backup original
    backup = meta_path.parent / 'meta_backup' / meta_path.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        meta_path.replace(backup)
        # recreate meta_path with binary content
        write_binary_meta(meta_path, magic, indices)
        print('Backed up text meta to', backup)
        print('Wrote binary meta with', len(indices), 'indices to', meta_path)
    else:
        # If backup exists, just overwrite meta_path
        write_binary_meta(meta_path, magic, indices)
        print('Overwrote meta with binary (backup already exists at', backup, ')')

if __name__ == '__main__':
    main()
