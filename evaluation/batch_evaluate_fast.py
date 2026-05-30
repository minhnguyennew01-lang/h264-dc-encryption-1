#!/usr/bin/env python3
"""
Batch evaluation script using fast MD5 method (no ffmpeg)
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
    eval_script = Path(__file__).parent / "evaluate_fast.py"
    
    cmd = [
        "python3",
        str(eval_script),
        "--original", str(original_file),
        "--encrypted", str(files_dict["encrypted"]),
        "--decrypted", str(files_dict["decrypted"]),
    ]
    
    stdout, stderr, retcode = run_command(cmd, cwd=BASE_DIR)
    if retcode != 0:
        print(f"❌ Evaluation failed: {stderr}")
        return None
    
    # Parse JSON from output
    try:
        result = json.loads(stdout)
        result["encrypt_time_ms"] = files_dict["encrypt_time_ms"]
        result["decrypt_time_ms"] = files_dict["decrypt_time_ms"]
        result["total_time_ms"] = files_dict["encrypt_time_ms"] + files_dict["decrypt_time_ms"]
        return result
    except:
        print(f"Failed to parse evaluation result")
        return None


def main():
    print(f"🚀 Starting batch evaluation")
    print(f"📁 Base directory: {BASE_DIR}")
    print(f"🎬 Original file: {ORIGINAL_FILE}")
    print(f"Original file: {ORIGINAL_FILE}")
    if ORIGINAL_FILE.exists():
        size = os.path.getsize(ORIGINAL_FILE)
        print(f"File size: {size:,} bytes")
    else:
        print(f"ERROR: Original file not found!")
        return 1
    
    results = {}
    
    for strategy_name, strategy_config in STRATEGIES.items():
        # Encrypt and decrypt
        files = encrypt_decrypt_with_timing(strategy_name, strategy_config, ORIGINAL_FILE, KEY)
        if files is None:
            results[strategy_name] = {"status": "FAILED"}
            continue
        
        # Evaluate
        print(f"\n[3/3] Evaluating...")
        result = evaluate_strategy(strategy_name, ORIGINAL_FILE, files)
        
        if result is None:
            results[strategy_name] = {"status": "FAILED"}
        else:
            result["status"] = "SUCCESS"
            results[strategy_name] = result
            
            # Print summary
            print()
            print(f"✓ Perfect decryption: {result.get('perfect_decryption', False)}")
            print(f"  Encrypt: {result['encrypt_time_ms']:.2f} ms")
            print(f"  Decrypt: {result['decrypt_time_ms']:.2f} ms")
            print(f"  Total:   {result['total_time_ms']:.2f} ms")
    
    # Save results
    print()
    print(f"\n{'='*70}")
    print(f"SUMMARY")
    print(f"{'='*70}")
    
    for strategy_name in STRATEGIES.keys():
        result = results.get(strategy_name, {})
        status = result.get("status", "UNKNOWN")
        if status == "SUCCESS":
            perfect = "✓" if result.get("perfect_decryption") else "✗"
            print(f"{perfect} {strategy_name}: {status}")
        else:
            print(f"✗ {strategy_name}: {status}")
    
    # Save JSON results
    eval_dir = Path(__file__).parent
    for strategy_num in [1, 2, 3]:
        strategy_name = f"Strategy {strategy_num}"
        result = results.get(strategy_name)
        if result:
            output_file = eval_dir / f"result_strategy{strategy_num}.json"
            with open(output_file, 'w') as f:
                json.dump(result, f, indent=2)
            print(f"\n✓ {output_file}")
    
    print(f"\n✓ All result files saved to: {eval_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
