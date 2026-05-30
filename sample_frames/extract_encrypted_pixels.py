#!/usr/bin/env python3
"""
Trích xuất pixel data từ encrypted NALU payload
Mục đích: Lấy dữ liệu encrypted trực tiếp từ bitstream, không qua FFmpeg decode
"""

import struct
import sys
from pathlib import Path

def extract_nalu_payload(h264_file):
    """
    Tìm và trích xuất NALU payload từ H.264 file
    Returns: list of (nalu_type, payload_bytes)
    """
    with open(h264_file, 'rb') as f:
        data = f.read()
    
    nalus = []
    i = 0
    
    while i < len(data):
        # Tìm start code: 0x00000001 hoặc 0x000001
        if i + 3 < len(data) and data[i:i+4] == b'\x00\x00\x00\x01':
            start_pos = i + 4
            prefix_len = 4
        elif i + 2 < len(data) and data[i:i+3] == b'\x00\x00\x01':
            start_pos = i + 3
            prefix_len = 3
        else:
            i += 1
            continue
        
        # Tìm start code tiếp theo
        j = start_pos + 1
        next_start = len(data)
        while j < len(data) - 3:
            if j + 3 < len(data) and data[j:j+4] == b'\x00\x00\x00\x01':
                next_start = j
                break
            elif j + 2 < len(data) and data[j:j+3] == b'\x00\x00\x01':
                next_start = j
                break
            j += 1
        
        payload = data[start_pos:next_start]
        if payload:
            nalu_type = payload[0] & 0x1F
            nalus.append((nalu_type, payload))
        
        i = next_start if next_start < len(data) else len(data)
    
    return nalus

def create_image_from_payload(payload, width=352, height=288):
    """
    Tạo ảnh từ payload encrypted
    Cách 1: Lấy từng byte của payload làm giá trị pixel (grayscale)
    Cách 2: Reshape payload thành matrix
    """
    # Bỏ NAL header byte
    payload_data = payload[1:]
    
    # Lấy num_pixels từ kích thước payload
    num_pixels = len(payload_data)
    
    # Tạo ảnh có size phù hợp
    if num_pixels >= width * height:
        # Full frame
        pixels = payload_data[:width * height]
        actual_w, actual_h = width, height
    else:
        # Partial frame hoặc crop
        actual_h = num_pixels // width
        actual_w = width
        pixels = payload_data[:actual_w * actual_h]
    
    return pixels, actual_w, actual_h

def save_as_pgm(pixels, width, height, output_file):
    """Lưu ảnh grayscale dạng PGM format"""
    with open(output_file, 'wb') as f:
        # PGM header
        f.write(f"P5\n{width} {height}\n255\n".encode())
        # Pixel data
        f.write(pixels)

def extract_and_visualize(h264_file, output_prefix):
    """
    Trích xuất NALUs và tạo ảnh từ payload
    """
    print(f"\n📌 Phân tích: {Path(h264_file).name}")
    print("=" * 70)
    
    nalus = extract_nalu_payload(h264_file)
    print(f"Tổng NALUs: {len(nalus)}")
    
    # Phân loại NALUs
    nalu_types = {}
    for ntype, payload in nalus:
        if ntype not in nalu_types:
            nalu_types[ntype] = []
        nalu_types[ntype].append(payload)
    
    print(f"NALU types: {dict((k, len(v)) for k, v in sorted(nalu_types.items()))}")
    print(f"  Type 1 = Slice data (video)")
    print(f"  Type 5 = IDR (I-frame)")
    print(f"  Type 7 = SPS (header)")
    print(f"  Type 8 = PPS (header)\n")
    
    # Trích xuất slice NALUs (type 1, 5)
    slice_nalus = []
    for ntype, payloads in nalu_types.items():
        if ntype in [1, 5]:  # Slice types
            slice_nalus.extend(payloads)
    
    if not slice_nalus:
        print("❌ Không tìm thấy slice NALUs")
        return
    
    # Lấy NALU đầu tiên (hoặc NALU lớn nhất)
    largest_nalu = max(slice_nalus, key=len)
    largest_idx = slice_nalus.index(largest_nalu)
    
    print(f"Slice NALU sizes: {sorted([len(p) for p in slice_nalus])}")
    print(f"Lấy NALU #{largest_idx} (size={len(largest_nalu)} bytes)\n")
    
    # Cách 1: Byte trực tiếp từ payload
    print("💾 Cách 1: Lấy byte trực tiếp từ encrypted payload")
    payload_data = largest_nalu[1:]  # Bỏ NAL header
    
    # Tạo ảnh có aspect ratio chuẩn H.264 (352x288 hoặc 640x480)
    width, height = 352, 288
    target_pixels = width * height
    
    if len(payload_data) >= target_pixels:
        pixels = payload_data[:target_pixels]
        output_pgm = f"{output_prefix}_raw_encrypted.pgm"
        save_as_pgm(pixels, width, height, output_pgm)
        print(f"  ✅ Saved: {output_pgm}")
    else:
        print(f"  ⚠️  Payload quá nhỏ ({len(payload_data)} < {target_pixels})")
    
    # Cách 2: Reshape dữ liệu thành square
    print("\n💾 Cách 2: Reshape payload thành square image")
    import math
    num_bytes = len(payload_data)
    sq_size = int(math.sqrt(num_bytes))
    pixels_square = payload_data[:sq_size * sq_size]
    output_square = f"{output_prefix}_raw_square.pgm"
    save_as_pgm(pixels_square, sq_size, sq_size, output_square)
    print(f"  ✅ Saved: {output_square} ({sq_size}x{sq_size})")
    
    # Phân tích entropy
    print("\n📊 Phân tích byte distribution:")
    byte_hist = {}
    for b in payload_data:
        byte_hist[b] = byte_hist.get(b, 0) + 1
    
    unique_bytes = len(byte_hist)
    print(f"  Unique bytes: {unique_bytes}/256")
    print(f"  Total bytes: {len(payload_data)}")
    
    # Entropy calculation
    entropy = 0
    for count in byte_hist.values():
        p = count / len(payload_data)
        if p > 0:
            entropy -= p * (2**0.5 if p > 0 else 0).__mul__(p).__truediv__(len(payload_data)).__mul__(8) if False else p * (-1 * (p / len(payload_data))**0.5 if p > 0 else 0)
    
    import math
    entropy = 0
    for count in byte_hist.values():
        p = count / len(payload_data)
        if p > 0:
            entropy -= p * math.log2(p)
    
    print(f"  Entropy: {entropy:.4f} bits/byte")
    
    # Top 10 values
    print("\n  Top 10 byte values:")
    sorted_bytes = sorted(byte_hist.items(), key=lambda x: -x[1])
    for byte_val, count in sorted_bytes[:10]:
        print(f"    Byte 0x{byte_val:02X} ({byte_val:3d}): {count:6d} times ({100*count/len(payload_data):5.2f}%)")

if __name__ == "__main__":
    import glob
    
    sample_dir = Path(__file__).parent
    
    # Xử lý tất cả encrypted files
    encrypted_files = list(sample_dir.glob("*_encrypted_*.h264"))
    
    if not encrypted_files:
        print("❌ Không tìm thấy *_encrypted_*.h264 files")
        sys.exit(1)
    
    for h264_file in sorted(encrypted_files):
        try:
            # Lấy prefix từ tên file (ví dụ "01_encrypted_s1" -> "01_encrypted_s1")
            prefix = str(h264_file).replace(".h264", "")
            extract_and_visualize(h264_file, prefix)
        except Exception as e:
            print(f"❌ Lỗi xử lý {h264_file}: {e}")
            import traceback
            traceback.print_exc()
