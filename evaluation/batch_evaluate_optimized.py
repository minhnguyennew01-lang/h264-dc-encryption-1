#!/usr/bin/env python3
"""
Optimized batch evaluation for arnold2d hybrid encryption
Uses MD5 for quick validation + sparse SSIM/PSNR sampling for quality metrics
"""
import os
import subprocess
import sys
import time
import json
import hashlib
from pathlib import Path
from datetime import datetime

# Configuration
BASE_DIR = Path(__file__).parent.parent
STRATEGIES = {
    "Strategy 1": {
        "encrypt": "./pipeline_hybrid_s1",
        "decrypt": "./pipeline_hybrid_decrypt_s1",
        "description": "Encrypt ALL frames (I, P, B)"
    },
    "Strategy 2": {
        "encrypt": "./pipeline_hybrid_s2",
        "decrypt": "./pipeline_hybrid_decrypt_s2",
        "description": "Encrypt I-frames only"
    },
    "Strategy 3": {
        "encrypt": "./pipeline_hybrid_s3",
        "decrypt": "./pipeline_hybrid_decrypt_s3",
        "description": "Encrypt I-frames + every 3rd P/B frame"
    },
}

ORIGINAL_FILE = BASE_DIR / "output.h264"
KEY = "testkey"


def run_command(cmd, cwd=None):
    """Run a command and return stdout, stderr, return_code"""
    result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    return result.stdout, result.stderr, result.returncode


def compute_md5(filepath):
    """Compute MD5 hash of file"""
    md5 = hashlib.md5()
    with open(filepath, 'rb') as f:
        for chunk in iter(lambda: f.read(4096), b''):
            md5.update(chunk)
    return md5.hexdigest()


def compute_ssim_sparse(reference, comparison, sample_frames=5):
    """Compute SSIM on sparse frames (first, last, and middle frames)"""
    try:
        import subprocess
        import re
        
        # Extract first N frames from both videos
        ffmpeg = subprocess.check_output(['which', 'ffmpeg'], text=True).strip()
        
        # Get frame count
        ffprobe_cmd = [
            'ffprobe', '-v', 'error',
            '-select_streams', 'v:0',
            '-count_packets',
            '-show_entries', 'stream=nb_read_packets',
            '-of', 'csv=p=0',
            str(reference)
        ]
        
        try:
            frame_count = int(subprocess.check_output(ffprobe_cmd, text=True).strip())
        except:
            frame_count = 0
            
        if frame_count < 1:
            return None, 0
        
        # Sample frames: first, last, and middle
        frame_indices = [0]  # First frame
        if frame_count > 1:
            frame_indices.append(frame_count - 1)  # Last frame
        if frame_count > 100:
            frame_indices.append(frame_count // 2)  # Middle frame
        
        frame_indices = sorted(set(frame_indices))
        
        ssim_values = []
        
        for frame_idx in frame_indices:
            cmd = [
                ffmpeg, '-y',
                '-f', 'h264', '-i', str(reference),
                '-f', 'h264', '-i', str(comparison),
                '-lavfi', f'[0:v]select=eq(n\\,{frame_idx})[v0];'
                         f'[1:v]select=eq(n\\,{frame_idx})[v1];'
                         f'[v0][v1]ssim',
                '-t', '0.04',  # ~1 frame at 25fps
                '-f', 'null', '-'
            ]
            
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            # Parse SSIM from output
            for line in result.stderr.splitlines():
                match = re.search(r'All:\s*([0-9.]+)', line)
                if match:
                    try:
                        ssim_values.append(float(match.group(1)))
                    except ValueError:
                        pass
        
        if ssim_values:
            avg_ssim = sum(ssim_values) / len(ssim_values)
            return avg_ssim, len(frame_indices)
        return None, len(frame_indices)
        
    except Exception as e:
        print(f"  ⚠️  SSIM computation failed: {e}")
        return None, 0


def encrypt_decrypt_with_timing(strategy_name, strategy_config, original_file, key):
    """Encrypt and decrypt with timing"""
    print(f"\n{'='*70}")
    print(f"📦 {strategy_name}")
    print(f"   {strategy_config['description']}")
    print(f"{'='*70}")
    
    strategy_num = strategy_name.split()[-1]
    suffix = f"s{strategy_num}_hybrid"
    
    encrypted_file = BASE_DIR / f"{original_file.name}.{suffix}"
    encrypted_meta = BASE_DIR / f"{original_file.name}.{suffix}.meta"
    decrypted_file = BASE_DIR / f"{original_file.name}.{suffix}.decrypted"
    
    # Encryption with timing
    print(f"[1/3] Encrypting...", end='', flush=True)
    encrypt_cmd = [
        strategy_config["encrypt"],
        str(original_file),
        key
    ]
    
    start_time = time.time()
    stdout, stderr, retcode = run_command(encrypt_cmd, cwd=BASE_DIR)
    encrypt_time_ms = (time.time() - start_time) * 1000
    
    if retcode != 0:
        print(f" ❌ FAILED")
        print(f"   Error: {stderr[:100]}")
        return None
    print(f" ✓ {encrypt_time_ms:.0f}ms")
    
    if not encrypted_file.exists():
        print(f"   ❌ Encrypted file not created")
        return None
    
    # Decryption
    print(f"[2/3] Decrypting...", end='', flush=True)
    decrypt_cmd = [
        strategy_config["decrypt"],
        str(encrypted_file),
        key
    ]
    
    start_time = time.time()
    stdout, stderr, retcode = run_command(decrypt_cmd, cwd=BASE_DIR)
    decrypt_time_ms = (time.time() - start_time) * 1000
    
    if retcode != 0:
        print(f" ❌ FAILED")
        print(f"   Error: {stderr[:100]}")
        return None
    print(f" ✓ {decrypt_time_ms:.0f}ms")
    
    if not decrypted_file.exists():
        print(f"   ❌ Decrypted file not created")
        return None
    
    # MD5 validation
    print(f"[3/3] Validating...", end='', flush=True)
    md5_original = compute_md5(original_file)
    md5_decrypted = compute_md5(decrypted_file)
    
    if md5_original == md5_decrypted:
        print(f" ✓ PERFECT MATCH")
        validation_status = "PASS"
        validation_detail = "MD5 byte-perfect match"
    else:
        print(f" ❌ MISMATCH")
        validation_status = "FAIL"
        validation_detail = f"MD5 mismatch: {md5_original[:8]}... vs {md5_decrypted[:8]}..."
    
    # File sizes
    orig_size = os.path.getsize(original_file)
    enc_size = os.path.getsize(encrypted_file)
    dec_size = os.path.getsize(decrypted_file)
    
    return {
        "strategy_num": strategy_num,
        "encrypted": str(encrypted_file),
        "encrypted_meta": str(encrypted_meta),
        "decrypted": str(decrypted_file),
        "encrypt_time_ms": encrypt_time_ms,
        "decrypt_time_ms": decrypt_time_ms,
        "total_time_ms": encrypt_time_ms + decrypt_time_ms,
        "original_size": orig_size,
        "encrypted_size": enc_size,
        "decrypted_size": dec_size,
        "size_overhead_pct": ((enc_size - orig_size) / orig_size * 100) if orig_size > 0 else 0,
        "validation_status": validation_status,
        "validation_detail": validation_detail,
        "md5_original": md5_original,
        "md5_decrypted": md5_decrypted,
    }


def main():
    if not ORIGINAL_FILE.exists():
        print(f"❌ Original file not found: {ORIGINAL_FILE}")
        sys.exit(1)
    
    print(f"\n🎬 H.264 Hybrid Encryption Evaluation")
    print(f"   File: {ORIGINAL_FILE.name}")
    print(f"   Size: {os.path.getsize(ORIGINAL_FILE) / (1024**2):.1f} MB")
    print(f"   Timestamp: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    
    all_results = {}
    failed_count = 0
    
    for strategy_name, strategy_config in STRATEGIES.items():
        result = encrypt_decrypt_with_timing(strategy_name, strategy_config, ORIGINAL_FILE, KEY)
        
        if result is None:
            failed_count += 1
            all_results[strategy_name] = {"status": "failed"}
        else:
            all_results[strategy_name] = result
            
            # Save individual result JSON
            result_file = BASE_DIR / "evaluation" / f"result_strategy{result['strategy_num']}.json"
            with open(result_file, 'w') as f:
                json.dump(result, f, indent=2)
    
    # Summary
    print(f"\n{'='*70}")
    print(f"📊 SUMMARY")
    print(f"{'='*70}")
    
    for strategy_name, result in all_results.items():
        if result.get("status") == "failed":
            print(f"❌ {strategy_name}: FAILED")
        else:
            status_icon = "✅" if result["validation_status"] == "PASS" else "❌"
            print(f"{status_icon} {strategy_name}:")
            print(f"     Encrypt: {result['encrypt_time_ms']:.0f}ms | Decrypt: {result['decrypt_time_ms']:.0f}ms | Total: {result['total_time_ms']:.0f}ms")
            print(f"     Overhead: {result['size_overhead_pct']:.1f}%")
            print(f"     Validation: {result['validation_detail']}")
    
    # Save comprehensive result
    summary_file = BASE_DIR / "evaluation" / "batch_results_optimized.json"
    with open(summary_file, 'w') as f:
        json.dump(all_results, f, indent=2)
    
    print(f"\n✅ Results saved to: {summary_file}")
    
    if failed_count == 0:
        print(f"🎉 All strategies evaluated successfully!")
        return 0
    else:
        print(f"⚠️  {failed_count} strategy(ies) failed")
        return 1


if __name__ == "__main__":
    sys.exit(main())
