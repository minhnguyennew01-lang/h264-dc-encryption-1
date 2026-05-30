#!/usr/bin/env python3
"""
Batch evaluation script for all three strategies with timing
"""
import os
import subprocess
import sys
import time
import json
from pathlib import Path

# Configuration
BASE_DIR = Path(__file__).parent.parent
STRATEGIES = {
    "Strategy 1": {
        "encrypt": "./pipeline_hybrid_s1",
        "decrypt": "./pipeline_hybrid_decrypt_s1",
    },
    "Strategy 2": {
        "encrypt": "./pipeline_hybrid_s2",
        "decrypt": "./pipeline_hybrid_decrypt_s2",
    },
    "Strategy 3": {
        "encrypt": "./pipeline_hybrid_s3",
        "decrypt": "./pipeline_hybrid_decrypt_s3",
    },
}

ORIGINAL_FILE = BASE_DIR / "output.h264"
KEY = "testkey"


def run_command(cmd, cwd=None):
    """Run a command and return stdout, stderr, return_code"""
    result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    return result.stdout, result.stderr, result.returncode


def encrypt_decrypt_with_timing(strategy_name, strategy_config, original_file, key):
    """Encrypt and decrypt with timing"""
    print(f"\n{'='*70}")
    print(f"Processing: {strategy_name}")
    print(f"{'='*70}")
    
    strategy_num = strategy_name.split()[-1]  # "1", "2", or "3"
    suffix = f"s{strategy_num}_hybrid"
    
    # Pipeline creates files with full filename as base (including .h264)
    # output.h264 -> output.h264.s1_hybrid, output.h264.s1_hybrid.decrypted
    encrypted_file = BASE_DIR / f"{original_file.name}.{suffix}"
    encrypted_meta = BASE_DIR / f"{original_file.name}.{suffix}.meta"
    decrypted_file = BASE_DIR / f"{original_file.name}.{suffix}.decrypted"
    
    # Encryption with timing
    print(f"[1/2] Encrypting...")
    encrypt_cmd = [
        strategy_config["encrypt"],
        str(original_file),
        key
    ]
    
    start_time = time.time()
    stdout, stderr, retcode = run_command(encrypt_cmd, cwd=BASE_DIR)
    encrypt_time_ms = (time.time() - start_time) * 1000
    
    if retcode != 0:
        print(f"❌ Encryption failed: {stderr}")
        return None
    print(f"✓ Encrypted in {encrypt_time_ms:.2f} ms")
    print(stderr.strip() if stderr else stdout.strip())
    
    # Verify encrypted file exists
    if not encrypted_file.exists():
        print(f"❌ Encrypted file not created: {encrypted_file}")
        return None
    
    # Decryption
    print(f"[2/2] Decrypting...")
    decrypt_cmd = [
        strategy_config["decrypt"],
        str(encrypted_file),
        key
    ]
    
    start_time = time.time()
    stdout, stderr, retcode = run_command(decrypt_cmd, cwd=BASE_DIR)
    decrypt_time_ms = (time.time() - start_time) * 1000
    
    if retcode != 0:
        print(f"❌ Decryption failed: {stderr}")
        return None
    print(f"✓ Decrypted in {decrypt_time_ms:.2f} ms")
    print(stderr.strip() if stderr else stdout.strip())
    
    # Verify decrypted file exists
    if not decrypted_file.exists():
        print(f"❌ Decrypted file not created: {decrypted_file}")
        return None
    
    return {
        "encrypted": encrypted_file,
        "encrypted_meta": encrypted_meta,
        "decrypted": decrypted_file,
        "encrypt_time_ms": encrypt_time_ms,
        "decrypt_time_ms": decrypt_time_ms,
    }


def evaluate_strategy(strategy_name, original_file, files_dict):
    """Evaluate a single strategy"""
    eval_script = Path(__file__).parent / "evaluate_enhanced.py"
    
    cmd = [
        "python3",
        str(eval_script),
        "--original", str(original_file),
        "--encrypted", str(files_dict["encrypted"]),
        "--decrypted", str(files_dict["decrypted"]),
        "--strategy", strategy_name,
        "--encryption-time-ms", str(files_dict["encrypt_time_ms"]),
        "--output-json", str(Path(__file__).parent / f"result_{strategy_name.split()[-1].lower()}.json"),
    ]
    
    stdout, stderr, retcode = run_command(cmd)
    if retcode != 0:
        print(f"❌ Evaluation failed: {stderr}")
        return None
    
    print(stdout)
    return f"result_{strategy_name.split()[-1].lower()}.json"


def main():
    if not ORIGINAL_FILE.exists():
        print(f"❌ Error: {ORIGINAL_FILE} not found")
        sys.exit(1)
    
    print(f"Original file: {ORIGINAL_FILE}")
    print(f"File size: {ORIGINAL_FILE.stat().st_size:,} bytes\n")
    
    results_summary = {}
    
    for strategy_name, strategy_config in STRATEGIES.items():
        # Encrypt and decrypt with timing
        files = encrypt_decrypt_with_timing(strategy_name, strategy_config, ORIGINAL_FILE, KEY)
        if files is None:
            results_summary[strategy_name] = "FAILED"
            continue
        
        # Evaluate
        result_file = evaluate_strategy(strategy_name, ORIGINAL_FILE, files)
        if result_file is None:
            results_summary[strategy_name] = "FAILED"
            continue
        
        results_summary[strategy_name] = "SUCCESS"
    
    # Print summary
    print(f"\n{'='*70}")
    print("SUMMARY")
    print(f"{'='*70}")
    for strategy, status in results_summary.items():
        status_symbol = "✓" if status == "SUCCESS" else "✗"
        print(f"{status_symbol} {strategy}: {status}")
    
    print(f"\nAll result files saved to: {Path(__file__).parent}")


if __name__ == "__main__":
    main()
