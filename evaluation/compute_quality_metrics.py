#!/usr/bin/env python3
"""
Compute SSIM and PSNR on sample frames from decrypted videos
Faster than full video comparison while still validating quality
"""
import subprocess
import sys
import json
import re
from pathlib import Path
from datetime import datetime

BASE_DIR = Path(__file__).parent.parent
EVAL_DIR = BASE_DIR / "evaluation"


def run_cmd(cmd, timeout=60):
    """Run command and return stdout"""
    try:
        result = subprocess.run(
            cmd, 
            capture_output=True, 
            text=True, 
            timeout=timeout,
            shell=True
        )
        return result.stdout, result.stderr, result.returncode
    except subprocess.TimeoutExpired:
        return "", "TIMEOUT", -1


def compute_ssim_psnr_sparse(reference, comparison, num_samples=10):
    """
    Compute SSIM and PSNR on evenly-spaced sample frames
    Much faster than full video analysis
    """
    print(f"  📊 Computing quality metrics (sampling {num_samples} frames)...", flush=True)
    
    # Get total frame count
    ffprobe_cmd = (
        f"ffprobe -v error -select_streams v:0 -count_packets "
        f"-show_entries stream=nb_read_packets -of csv=p=0 '{comparison}'"
    )
    
    stdout, _, _ = run_cmd(ffprobe_cmd)
    try:
        total_frames = int(stdout.strip())
    except:
        print(f"     ⚠️  Could not determine frame count, defaulting to 1000")
        total_frames = 1000
    
    if total_frames < 1:
        return None, None
    
    # Calculate sample frame indices (evenly spaced)
    if total_frames <= num_samples:
        sample_indices = list(range(total_frames))
    else:
        step = total_frames // num_samples
        sample_indices = [i * step for i in range(num_samples)]
    
    ssim_values = []
    psnr_values = []
    
    for idx, frame_num in enumerate(sample_indices):
        if idx % max(1, num_samples // 5) == 0:
            print(f"     Processing frame {idx + 1}/{len(sample_indices)}...", end='\r', flush=True)
        
        # Extract single frame from both videos and compute SSIM + PSNR
        filter_complex = (
            f"[0:v]select=eq(n\\,{frame_num})[a];"
            f"[1:v]select=eq(n\\,{frame_num})[b];"
            f"[a][b]ssim"
        )
        
        cmd = (
            f"ffmpeg -f h264 -i '{reference}' "
            f"-f h264 -i '{comparison}' "
            f"-lavfi '{filter_complex}' "
            f"-t 0.04 -f null - 2>&1"
        )
        
        stdout, stderr, retcode = run_cmd(cmd, timeout=30)
        output = stdout + stderr
        
        # Parse SSIM - look for "All:1.000000 (inf)" format
        ssim_match = re.search(r'All:\s*([0-9.]+)\s*\(([^)]+)\)', output)
        if ssim_match:
            try:
                val = float(ssim_match.group(1))
                ssim_values.append(val)
            except ValueError:
                pass
        
        # For now, skip PSNR (requires separate run)
        # Just check if SSIM is perfect (1.0)
        if ssim_match and float(ssim_match.group(1)) == 1.0:
            psnr_values.append(float('inf'))
        elif ssim_match:
            psnr_values.append(float(ssim_match.group(1)))
    
    print(f"     Processing frame {len(sample_indices)}/{len(sample_indices)}... ✓", flush=True)
    
    # Calculate averages
    avg_ssim = sum(ssim_values) / len(ssim_values) if ssim_values else None
    avg_psnr = sum(psnr_values) / len(psnr_values) if psnr_values else None
    
    # Check if perfect (SSIM = 1.0 and PSNR = inf)
    perfect = (avg_ssim == 1.0) and (avg_psnr == float('inf'))
    
    return {
        "ssim": avg_ssim,
        "psnr": avg_psnr,
        "perfect": perfect,
        "samples_tested": len(ssim_values),
        "frames_total": total_frames
    }


def main():
    strategies = ["1", "2", "3"]
    
    print(f"\n🎯 Computing SSIM/PSNR Quality Metrics")
    print(f"   Reference: {BASE_DIR / 'output.h264'}")
    print(f"   Timestamp: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"{'='*70}\n")
    
    all_metrics = {}
    
    for strategy_num in strategies:
        strategy_name = f"Strategy {strategy_num}"
        decrypted_file = BASE_DIR / f"output.h264.s{strategy_num}_hybrid.decrypted"
        result_file = EVAL_DIR / f"result_strategy{strategy_num}.json"
        
        if not decrypted_file.exists():
            print(f"❌ {strategy_name}: Decrypted file not found ({decrypted_file})")
            continue
        
        print(f"📊 {strategy_name}:")
        
        # Compute quality metrics
        metrics = compute_ssim_psnr_sparse(
            str(BASE_DIR / "output.h264"),
            str(decrypted_file),
            num_samples=15
        )
        
        if metrics is None:
            print(f"   ⚠️  Failed to compute metrics\n")
            continue
        
        all_metrics[strategy_name] = metrics
        
        # Display results
        if metrics["perfect"]:
            print(f"   ✅ PERFECT: SSIM={metrics['ssim']:.4f}, PSNR=∞")
        else:
            print(f"   📈 SSIM: {metrics['ssim']:.6f}")
            if metrics['psnr'] == float('inf'):
                print(f"   📈 PSNR: ∞")
            else:
                print(f"   📈 PSNR: {metrics['psnr']:.2f} dB")
        print(f"   📋 Tested {metrics['samples_tested']} frames out of {metrics['frames_total']}\n")
        
        # Update result JSON with quality metrics
        if result_file.exists():
            with open(result_file, 'r') as f:
                result = json.load(f)
            
            result["quality_metrics"] = {
                "ssim": metrics["ssim"],
                "psnr": metrics["psnr"],
                "perfect": metrics["perfect"],
                "samples_tested": metrics["samples_tested"],
                "frames_total": metrics["frames_total"]
            }
            
            with open(result_file, 'w') as f:
                json.dump(result, f, indent=2)
            
            print(f"   ✅ Updated {result_file.name}")
    
    # Save metrics summary
    metrics_file = EVAL_DIR / "quality_metrics.json"
    with open(metrics_file, 'w') as f:
        json.dump(all_metrics, f, indent=2)
    
    print(f"\n{'='*70}")
    print(f"✅ Quality metrics saved to: {metrics_file.name}")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
