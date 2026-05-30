#!/usr/bin/env python3
"""
Fast evaluation using MD5 + file size (no ffmpeg)
Much faster for large files
"""
import argparse
import hashlib
import json
import os
from pathlib import Path

def compute_md5(filepath):
    """Compute MD5 hash of a file"""
    md5_hash = hashlib.md5()
    with open(filepath, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            md5_hash.update(chunk)
    return md5_hash.hexdigest()

def evaluate_pair(original_file, encrypted_file, decrypted_file):
    """Evaluate encryption/decryption pair"""
    if not os.path.exists(original_file):
        raise FileNotFoundError(f"Original file not found: {original_file}")
    if not os.path.exists(encrypted_file):
        raise FileNotFoundError(f"Encrypted file not found: {encrypted_file}")
    if not os.path.exists(decrypted_file):
        raise FileNotFoundError(f"Decrypted file not found: {decrypted_file}")
    
    # Get file info
    orig_size = os.path.getsize(original_file)
    enc_size = os.path.getsize(encrypted_file)
    dec_size = os.path.getsize(decrypted_file)
    
    # Compute MD5
    print(f"Computing MD5 for original: {original_file}...")
    orig_md5 = compute_md5(original_file)
    
    print(f"Computing MD5 for decrypted: {decrypted_file}...")
    dec_md5 = compute_md5(decrypted_file)
    
    # Check if decryption is perfect
    perfect_decryption = orig_md5 == dec_md5
    
    # Size change
    size_change_percent = ((enc_size - orig_size) / orig_size * 100) if orig_size > 0 else 0
    
    result = {
        "original_file": str(original_file),
        "encrypted_file": str(encrypted_file),
        "decrypted_file": str(decrypted_file),
        "original_size_bytes": orig_size,
        "encrypted_size_bytes": enc_size,
        "decrypted_size_bytes": dec_size,
        "original_md5": orig_md5,
        "decrypted_md5": dec_md5,
        "perfect_decryption": perfect_decryption,
        "size_change_percent": round(size_change_percent, 2),
        "ssim_original_encrypted": "N/A",  # Encrypted not decodable
        "psnr_original_encrypted": "N/A",  # Encrypted not decodable
        "ssim_original_decrypted": "1.0" if perfect_decryption else "0.0",
        "psnr_original_decrypted": "inf" if perfect_decryption else "0.0",
    }
    
    return result

def main():
    parser = argparse.ArgumentParser(description="Fast evaluation of encrypted/decrypted files")
    parser.add_argument("--original", required=True, help="Original file path")
    parser.add_argument("--encrypted", required=True, help="Encrypted file path")
    parser.add_argument("--decrypted", required=True, help="Decrypted file path")
    parser.add_argument("--output", help="Output JSON file path")
    
    args = parser.parse_args()
    
    result = evaluate_pair(args.original, args.encrypted, args.decrypted)
    
    # Print result
    print("\n" + "="*70)
    print("EVALUATION RESULT")
    print("="*70)
    print(f"Original MD5:  {result['original_md5']}")
    print(f"Decrypted MD5: {result['decrypted_md5']}")
    print(f"Perfect:       {'✓ YES' if result['perfect_decryption'] else '✗ NO'}")
    print(f"Size change:   {result['size_change_percent']:+.2f}%")
    print()
    
    # Save JSON
    if args.output:
        with open(args.output, 'w') as f:
            json.dump(result, f, indent=2)
        print(f"✓ Result saved to: {args.output}")
    else:
        print(json.dumps(result, indent=2))

if __name__ == "__main__":
    main()
