#!/usr/bin/env python3
import argparse
import json
import os
import re
import shutil
import subprocess
import sys

FFMPEG_BINARY = shutil.which("ffmpeg")
FFPROBE_BINARY = shutil.which("ffprobe")

SSIM_PATTERN = re.compile(r"All:\s*([0-9.]+)")
PSNR_PATTERN = re.compile(r"average:\s*([0-9.]+|inf|INF)")


def check_binary(binary, name):
    if binary is None:
        raise RuntimeError(f"{name} is required but was not found in PATH. Install ffmpeg.")


def ffmpeg_input_args(path):
    ext = os.path.splitext(path)[1].lower()
    if ext in (".h264", ".264"):
        return ["-f", "h264", "-i", path]
    return ["-i", path]


def run_command(cmd):
    result = subprocess.run(cmd, capture_output=True, text=True)
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
    if FFPROBE_BINARY:
        try:
            cmd = [FFPROBE_BINARY, "-v", "error", "-select_streams", "v:0", "-show_entries",
                   "format=size,bit_rate,duration", "-of", "default=noprint_wrappers=1:nokey=1", path]
            out, err = run_command(cmd)
            lines = [line.strip() for line in out.splitlines() if line.strip()]
            if len(lines) >= 3:
                info["file_size_probe"] = int(lines[0]) if lines[0].isdigit() else None
                info["bit_rate"] = int(lines[1]) if lines[1].isdigit() else None
                try:
                    info["duration"] = float(lines[2])
                except ValueError:
                    info["duration"] = None
        except Exception:
            pass
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


def compute_ssim(reference, comparison):
    check_binary(FFMPEG_BINARY, "ffmpeg")
    cmd = [FFMPEG_BINARY, "-y"]
    cmd += ffmpeg_input_args(reference)
    cmd += ffmpeg_input_args(comparison)
    cmd += ["-lavfi", "[0:v][1:v]ssim=stats_file=/dev/null", "-f", "null", "-"]
    try:
        out, err = run_command(cmd)
    except RuntimeError as exc:
        # ffmpeg writes metrics to stderr
        err = exc.args[0].split("stderr:\n", 1)[-1]
        out = ""
    ssim_value = parse_ssim_output(err)
    return ssim_value


def compute_psnr(reference, comparison):
    check_binary(FFMPEG_BINARY, "ffmpeg")
    cmd = [FFMPEG_BINARY, "-y"]
    cmd += ffmpeg_input_args(reference)
    cmd += ffmpeg_input_args(comparison)
    cmd += ["-lavfi", "[0:v][1:v]psnr=stats_file=/dev/null", "-f", "null", "-"]
    try:
        out, err = run_command(cmd)
    except RuntimeError as exc:
        err = exc.args[0].split("stderr:\n", 1)[-1]
        out = ""
    psnr_value = parse_psnr_output(err)
    return psnr_value


def compute_metrics(original, encrypted=None, decrypted=None):
    metrics = {
        "original": get_file_info(original)
    }
    if encrypted:
        metrics["encrypted"] = get_file_info(encrypted)
        metrics["encrypted_ratio"] = metrics["encrypted"]["size_bytes"] / metrics["original"]["size_bytes"]
        metrics["encrypted_size_change_pct"] = (metrics["encrypted"]["size_bytes"] - metrics["original"]["size_bytes"]) / metrics["original"]["size_bytes"] * 100
        metrics["ssim_original_encrypted"] = compute_ssim(original, encrypted)
        metrics["psnr_original_encrypted"] = compute_psnr(original, encrypted)
    if decrypted:
        metrics["decrypted"] = get_file_info(decrypted)
        metrics["decrypted_ratio"] = metrics["decrypted"]["size_bytes"] / metrics["original"]["size_bytes"]
        metrics["decrypted_size_change_pct"] = (metrics["decrypted"]["size_bytes"] - metrics["original"]["size_bytes"]) / metrics["original"]["size_bytes"] * 100
        metrics["ssim_original_decrypted"] = compute_ssim(original, decrypted)
        metrics["psnr_original_decrypted"] = compute_psnr(original, decrypted)
    return metrics


def print_metrics(metrics, strategy=None):
    if strategy:
        print(f"\n=== Evaluation for strategy: {strategy} ===")
    print(f"Original file: {metrics['original']['path']}")
    print(f"  Size: {metrics['original']['size_bytes']} bytes")
    if "encrypted" in metrics:
        print(f"Encrypted file: {metrics['encrypted']['path']}")
        print(f"  Size: {metrics['encrypted']['size_bytes']} bytes")
        print(f"  Size change: {metrics['encrypted_size_change_pct']:.4f}%")
        print(f"  SSIM original->encrypted: {metrics['ssim_original_encrypted']}")
        print(f"  PSNR original->encrypted: {metrics['psnr_original_encrypted']}")
    if "decrypted" in metrics:
        print(f"Decrypted file: {metrics['decrypted']['path']}")
        print(f"  Size: {metrics['decrypted']['size_bytes']} bytes")
        print(f"  Size change: {metrics['decrypted_size_change_pct']:.4f}%")
        print(f"  SSIM original->decrypted: {metrics['ssim_original_decrypted']}")
        print(f"  PSNR original->decrypted: {metrics['psnr_original_decrypted']}")
    print("")


def main():
    parser = argparse.ArgumentParser(description="Evaluate H.264 selective encryption metrics.")
    parser.add_argument("--original", required=True, help="Original reference H.264 file")
    parser.add_argument("--encrypted", help="Encrypted H.264 file")
    parser.add_argument("--decrypted", help="Decrypted H.264 file")
    parser.add_argument("--strategy", help="Name of the evaluated strategy")
    parser.add_argument("--output-json", help="Write results to JSON file")
    args = parser.parse_args()

    try:
        metrics = compute_metrics(args.original, args.encrypted, args.decrypted)
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
