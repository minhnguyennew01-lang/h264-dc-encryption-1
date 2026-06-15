#!/usr/bin/env python3
import sys
import struct
from pathlib import Path

def read_nalu_info(path: Path):
    data = path.read_bytes()
    if len(data) < 4:
        raise RuntimeError('nalu_info.bin too small')
    off = 0
    count = struct.unpack_from('<I', data, off)[0]
    off += 4
    nalus = []
    for i in range(count):
        if off + 9 > len(data):
            break
        start = struct.unpack_from('<I', data, off)[0]; off += 4
        nal = struct.unpack_from('<I', data, off)[0]; off += 4
        typ = struct.unpack_from('<B', data, off)[0]; off += 1
        nalus.append((start, nal, typ))
    return nalus

def build_indices(nalus, strategy):
    indices = []
    if strategy == 'S1':
        for i, (_, _, t) in enumerate(nalus):
            if t == 1 or t == 5:
                indices.append(i)
    elif strategy == 'S2':
        for i, (_, _, t) in enumerate(nalus):
            if t == 5:
                indices.append(i)
    elif strategy == 'S3':
        frame_counter = 0
        for i, (_, _, t) in enumerate(nalus):
            if frame_counter % 3 == 0:
                indices.append(i)
            frame_counter += 1
    else:
        raise RuntimeError('Unknown strategy')
    return indices

def write_binary_meta(path: Path, magic: str, indices):
    with open(path, 'wb') as f:
        f.write(magic.encode('ascii'))
        for idx in indices:
            f.write(struct.pack('<I', idx))

def main():
    if len(sys.argv) < 3:
        print('Usage: generate_binary_meta_from_nalu.py <meta_out.meta> <S1|S2|S3> [nalu_info.bin]')
        sys.exit(1)
    out_meta = Path(sys.argv[1])
    strategy = sys.argv[2]
    nalu_file = Path(sys.argv[3]) if len(sys.argv) > 3 else Path('nalu_info.bin')
    if not nalu_file.exists():
        print('nalu_info.bin not found at', nalu_file)
        sys.exit(1)
    nalus = read_nalu_info(nalu_file)
    indices = build_indices(nalus, strategy)
    # backup
    backup_dir = out_meta.parent / 'meta_backup'
    backup_dir.mkdir(parents=True, exist_ok=True)
    backup = backup_dir / out_meta.name
    if not backup.exists() and out_meta.exists():
        out_meta.replace(backup)
        print('Backed up existing meta to', backup)
    write_binary_meta(out_meta, strategy + 'FX', indices)
    print(f'Wrote binary meta {out_meta} with {len(indices)} indices (strategy {strategy})')

if __name__ == '__main__':
    main()
