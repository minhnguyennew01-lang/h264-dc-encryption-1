#!/usr/bin/env python3
"""
Enhanced H.264 Encryption Evaluation Script
Computes SSIM, STRRED, file size overhead, and encryption timing.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time

FFMPEG_BINARY = shutil.which("ffmpeg")
FFPROBE_BINARY = shutil.which("ffprobe")

SSIM_PATTERN = re.compile(r"All:\s*([0-9.]+)")
PSNR_PATTERN = re.compile(r"average:\s*([0-9.]+|inf|INF)")
STRRED_PATTERN = re.compile(r"STRRED:\s*([0-9.]+)")


def check_binary(binary, name):
    if binary is None:
        raise RuntimeError(f"{name} is required but was not found in PATH. Install ffmpeg.")


def ffmpeg_input_args(path):
    ext = os.path.splitext(path)[1].lower()
    if ext in (".h264", ".264"):
        return ["-f", "h264", "-i", path]
    return ["-i", path]


def run_command(cmd, capture_output=True):
    result = subprocess.run(cmd, capture_output=capture_output, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"Command failed: {' '.join(cmd)}\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )
    return result.stdout, result.stderr


def get_file_info(path):
    if not os.path.exists(path):
        raise FileNotFoundError(path)
    size = os.path.getsize(path)
    info = {
        "path": os.path.abspath(path),
        "size_bytes": size,
    }
    return info


def parse_ssim_output(output):
    for line in output.splitlines():
        match = SSIM_PATTERN.search(line)
        if match:
            try:
                return float(match.group(1))
            except ValueError:
                continue
    return None


def parse_psnr_output(output):
    for line in output.splitlines():
        match = PSNR_PATTERN.search(line)
        if match:
            try:
                val = match.group(1)
                if val.lower() == "inf":
                    return float("inf")
                return float(val)
            except ValueError:
                continue
    return None


def parse_strred_output(output):
    """Parse STRRED value from ffmpeg output"""
    for line in output.splitlines():
        # STRRED appears in format like: STRRED: 10.5
        if "STRRED" in line:
            match = STRRED_PATTERN.search(line)
            if match:
                try:
                    return float(match.group(1))
                except ValueError:
                    continue
    return None


def compute_ssim(reference, comparison):
    """Compute SSIM between two video files"""
    check_binary(FFMPEG_BINARY, "ffmpeg")
    cmd = [FFMPEG_BINARY, "-y"]
    cmd += ffmpeg_input_args(reference)
    cmd += ffmpeg_input_args(comparison)
    cmd += ["-lavfi", "[0:v][1:v]ssim=stats_file=/dev/null", "-f", "null", "-"]
    try:
        out, err = run_command(cmd)
    except RuntimeError as exc:
        err = exc.args[0].split("stderr:\n", 1)[-1] if "stderr:\n" in exc.args[0] else str(exc)
        out = ""
    ssim_value = parse_ssim_output(err)
    return ssim_value


def compute_psnr(reference, comparison):
    """Compute PSNR between two video files"""
    check_binary(FFMPEG_BINARY, "ffmpeg")
    cmd = [FFMPEG_BINARY, "-y"]
    cmd += ffmpeg_input_args(reference)
    cmd += ffmpeg_input_args(comparison)
    cmd += ["-lavfi", "[0:v][1:v]psnr=stats_file=/dev/null", "-f", "null", "-"]
    try:
        out, err = run_command(cmd)
    except RuntimeError as exc:
        err = exc.args[0].split("stderr:\n", 1)[-1] if "stderr:\n" in exc.args[0] else str(exc)
        out = ""
    psnr_value = parse_psnr_output(err)
    return psnr_value


def compute_strred(reference, comparison, num_frames=30):
    """Compute STRRED (Spatio-Temporal Reduced Reference) between two video files"""
    check_binary(FFMPEG_BINARY, "ffmpeg")
    cmd = [FFMPEG_BINARY, "-y"]
    cmd += ffmpeg_input_args(reference)
    cmd += ffmpeg_input_args(comparison)
    # Extract first num_frames for faster computation
    cmd += ["-vframes", str(num_frames)]
    cmd += ["-lavfi", "[0:v][1:v]strred=stats_file=/dev/null", "-f", "null", "-"]
    try:
        out, err = run_command(cmd)
    except RuntimeError as exc:
        err = exc.args[0].split("stderr:\n", 1)[-1] if "stderr:\n" in exc.args[0] else str(exc)
        out = ""
    strred_value = parse_strred_output(err)
    return strred_value


def get_frame_count(path):
    """Get total frame count of a video file"""
    if not FFPROBE_BINARY:
        return None
    try:
        cmd = [FFPROBE_BINARY, "-v", "error", "-select_streams", "v:0",
               "-count_packets", "-show_entries", "stream=nb_read_packets",
               "-of", "default=noprint_wrappers=1:nokey=1", path]
        out, err = run_command(cmd)
        lines = [line.strip() for line in out.splitlines() if line.strip()]
        if lines and lines[0].isdigit():
            return int(lines[0])
    except Exception:
        pass
    return None


def compute_metrics(original, encrypted=None, decrypted=None, encryption_time=None, num_frames_strred=30):
    """Compute all metrics"""
    metrics = {
        "original": get_file_info(original),
        "frame_count": get_frame_count(original),
    }
    
    if encryption_time:
        metrics["encryption_time_ms"] = encryption_time
        frame_count = metrics.get("frame_count")
        if frame_count and frame_count > 0:
            metrics["encryption_time_per_frame_ms"] = encryption_time / frame_count
    
    if encrypted:
        metrics["encrypted"] = get_file_info(encrypted)
        metrics["encrypted_size_change_pct"] = (
            (metrics["encrypted"]["size_bytes"] - metrics["original"]["size_bytes"]) / 
            metrics["original"]["size_bytes"] * 100
        )
        # Try to compute STRRED if decryption is successful
        metrics["ssim_original_encrypted"] = compute_ssim(original, encrypted)
        metrics["psnr_original_encrypted"] = compute_psnr(original, encrypted)
        metrics["strred_original_encrypted"] = compute_strred(
            original, encrypted, num_frames=num_frames_strred
        )
    
    if decrypted:
        metrics["decrypted"] = get_file_info(decrypted)
        metrics["decrypted_size_change_pct"] = (
            (metrics["decrypted"]["size_bytes"] - metrics["original"]["size_bytes"]) / 
            metrics["original"]["size_bytes"] * 100
        )
        metrics["ssim_original_decrypted"] = compute_ssim(original, decrypted)
        metrics["psnr_original_decrypted"] = compute_psnr(original, decrypted)
        metrics["strred_original_decrypted"] = compute_strred(
            original, decrypted, num_frames=num_frames_strred
        )
    
    return metrics


def print_metrics(metrics, strategy=None):
    """Pretty print metrics"""
    if strategy:
        print(f"\n{'='*70}")
        print(f"Strategy: {strategy}")
        print(f"{'='*70}")
    
    print(f"Original file: {metrics['original']['path']}")
    print(f"  Size: {metrics['original']['size_bytes']:,} bytes")
    if metrics.get('frame_count'):
        print(f"  Frame count: {metrics['frame_count']}")
    
    if metrics.get('encryption_time_ms'):
        print(f"Encryption time: {metrics['encryption_time_ms']:.2f} ms")
        if metrics.get('encryption_time_per_frame_ms'):
            print(f"  Per frame: {metrics['encryption_time_per_frame_ms']:.2f} ms/frame")
    
    if "encrypted" in metrics:
        print(f"\nEncrypted file: {metrics['encrypted']['path']}")
        print(f"  Size: {metrics['encrypted']['size_bytes']:,} bytes")
        print(f"  Size change: {metrics['encrypted_size_change_pct']:.4f}%")
        print(f"  SSIM (original→encrypted): {metrics['ssim_original_encrypted']}")
        print(f"  PSNR (original→encrypted): {metrics['psnr_original_encrypted']}")
        print(f"  STRRED (original→encrypted): {metrics['strred_original_encrypted']}")
    
    if "decrypted" in metrics:
        print(f"\nDecrypted file: {metrics['decrypted']['path']}")
        print(f"  Size: {metrics['decrypted']['size_bytes']:,} bytes")
        print(f"  Size change: {metrics['decrypted_size_change_pct']:.4f}%")
        print(f"  SSIM (original→decrypted): {metrics['ssim_original_decrypted']}")
        print(f"  PSNR (original→decrypted): {metrics['psnr_original_decrypted']}")
        print(f"  STRRED (original→decrypted): {metrics['strred_original_decrypted']}")
    
    print()


def main():
    parser = argparse.ArgumentParser(
        description="Enhanced H.264 selective encryption evaluation (SSIM, PSNR, STRRED, timing)"
    )
    parser.add_argument("--original", required=True, help="Original reference H.264 file")
    parser.add_argument("--encrypted", help="Encrypted H.264 file")
    parser.add_argument("--decrypted", help="Decrypted H.264 file")
    parser.add_argument("--strategy", help="Name of the evaluated strategy")
    parser.add_argument("--encryption-time-ms", type=float, help="Encryption time in milliseconds")
    parser.add_argument("--num-frames-strred", type=int, default=30, 
                       help="Number of frames to use for STRRED computation (default: 30)")
    parser.add_argument("--output-json", help="Write results to JSON file")
    args = parser.parse_args()

    try:
        metrics = compute_metrics(
            args.original, 
            args.encrypted, 
            args.decrypted,
            encryption_time=args.encryption_time_ms,
            num_frames_strred=args.num_frames_strred
        )
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        sys.exit(1)

    print_metrics(metrics, args.strategy)
    
    if args.output_json:
        with open(args.output_json, "w", encoding="utf-8") as f:
            json.dump(metrics, f, indent=2)
        print(f"Results written to {args.output_json}")


if __name__ == "__main__":
    main()
