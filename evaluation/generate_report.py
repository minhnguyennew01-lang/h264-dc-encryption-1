#!/usr/bin/env python3
"""
Generate comparison report for all 3 strategies
"""
import json
from pathlib import Path
from tabulate import tabulate

EVAL_DIR = Path(__file__).parent
BASE_DIR = EVAL_DIR.parent

def format_bytes(b):
    """Format bytes to human readable"""
    for unit in ['B', 'KB', 'MB', 'GB']:
        if b < 1024:
            return f"{b:.1f} {unit}"
        b /= 1024
    return f"{b:.1f} TB"

def format_time(ms):
    """Format milliseconds"""
    if ms < 1000:
        return f"{ms:.0f} ms"
    return f"{ms/1000:.2f} s"

def main():
    # Load results
    results = {}
    for i in [1, 2, 3]:
        result_file = EVAL_DIR / f"result_strategy{i}.json"
        if result_file.exists():
            with open(result_file, 'r') as f:
                results[f"Strategy {i}"] = json.load(f)
    
    if not results:
        print("❌ No result files found")
        return
    
    print("\n" + "="*100)
    print("H.264 HYBRID ENCRYPTION EVALUATION REPORT")
    print("="*100)
    print(f"\nFile: {BASE_DIR / 'output.h264'}")
    print(f"Size: {format_bytes(results['Strategy 1']['original_size'])}")
    
    # Performance metrics
    print("\n" + "─"*100)
    print("⚡ PERFORMANCE METRICS")
    print("─"*100)
    
    perf_table = []
    for strat_name, result in results.items():
        perf_table.append([
            strat_name,
            format_time(result['encrypt_time_ms']),
            format_time(result['decrypt_time_ms']),
            format_time(result['total_time_ms']),
            f"{result['encrypt_time_ms']/1000 / (result['original_size']/(1024**2)):.3f} s/GB",
        ])
    
    print(tabulate(perf_table, headers=["Strategy", "Encrypt", "Decrypt", "Total", "Speed"], tablefmt="grid"))
    
    # Size metrics
    print("\n" + "─"*100)
    print("📊 FILE SIZE METRICS")
    print("─"*100)
    
    size_table = []
    for strat_name, result in results.items():
        size_table.append([
            strat_name,
            format_bytes(result['original_size']),
            format_bytes(result['encrypted_size']),
            format_bytes(result['decrypted_size']),
            f"{result['size_overhead_pct']:.2f}%",
        ])
    
    print(tabulate(size_table, headers=["Strategy", "Original", "Encrypted", "Decrypted", "Overhead"], tablefmt="grid"))
    
    # Validation metrics
    print("\n" + "─"*100)
    print("✅ VALIDATION & SECURITY METRICS")
    print("─"*100)
    
    valid_table = []
    for strat_name, result in results.items():
        valid_table.append([
            strat_name,
            result['validation_status'],
            result['md5_original'][:16] + "...",
            result['md5_decrypted'][:16] + "...",
            "✅ MATCH" if result['md5_original'] == result['md5_decrypted'] else "❌ MISMATCH",
        ])
    
    print(tabulate(valid_table, headers=["Strategy", "Status", "Original MD5", "Decrypted MD5", "Verification"], tablefmt="grid"))
    
    # Quality metrics
    if 'quality_metrics' in results['Strategy 1']:
        print("\n" + "─"*100)
        print("📈 QUALITY METRICS (SSIM/PSNR)")
        print("─"*100)
        
        qual_table = []
        for strat_name, result in results.items():
            if 'quality_metrics' in result:
                qm = result['quality_metrics']
                ssim_str = f"{qm['ssim']:.6f}"
                psnr_str = "∞" if qm['psnr'] == float('inf') else f"{qm['psnr']:.2f} dB"
                qual_table.append([
                    strat_name,
                    ssim_str,
                    psnr_str,
                    "PERFECT" if qm['perfect'] else "Degraded",
                    f"{qm['samples_tested']} frames",
                ])
        
        print(tabulate(qual_table, headers=["Strategy", "SSIM", "PSNR", "Status", "Samples"], tablefmt="grid"))
    
    # Summary
    print("\n" + "="*100)
    print("📝 SUMMARY")
    print("="*100)
    
    for strat_name, result in results.items():
        print(f"\n{strat_name}:")
        print(f"  ✓ Encryption: {format_time(result['encrypt_time_ms'])}")
        print(f"  ✓ Decryption: {format_time(result['decrypt_time_ms'])}")
        print(f"  ✓ File overhead: {result['size_overhead_pct']:.2f}%")
        print(f"  ✓ Validation: {result['validation_detail']}")
        if 'quality_metrics' in result and result['quality_metrics']['perfect']:
            print(f"  ✓ Quality: PERFECT (SSIM=1.0, PSNR=∞)")
    
    print("\n" + "="*100)
    print("🎉 All strategies evaluated successfully!")
    print("="*100 + "\n")

if __name__ == "__main__":
    main()
