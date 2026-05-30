#!/usr/bin/env python3
"""
Extract first I-frame from H.264 files
"""
import subprocess
import sys
from pathlib import Path

def extract_first_iframe(input_file, output_file):
    """Extract first I-frame as a frame image"""
    try:
        # Extract first I-frame as PNG image
        cmd = [
            "ffmpeg",
            "-i", str(input_file),
            "-vf", "select='eq(pict_type\\,I)',scale=320:-1",
            "-frames:v", "1",
            "-y",
            str(output_file)
        ]
        
        print(f"  Extracting I-frame from {input_file.name}...", end=" ", flush=True)
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        
        if result.returncode == 0:
            size = Path(output_file).stat().st_size if Path(output_file).exists() else 0
            print(f"✓ ({size:,} bytes)")
            return True
        else:
            print(f"✗ Error: {result.stderr[:100]}")
            return False
    except Exception as e:
        print(f"✗ Exception: {e}")
        return False


def main():
    base_dir = Path(__file__).parent
    sample_dir = base_dir / "sample_frames"
    
    if not sample_dir.exists():
        print(f"❌ Directory {sample_dir} not found!")
        return
    
    files = [
        ("00_original.h264", "00_original_iframe.png"),
        ("01_encrypted_s1.h264", "01_encrypted_s1_iframe.png"),
        ("01_decrypted_s1.h264", "01_decrypted_s1_iframe.png"),
        ("02_encrypted_s2.h264", "02_encrypted_s2_iframe.png"),
        ("02_decrypted_s2.h264", "02_decrypted_s2_iframe.png"),
        ("03_encrypted_s3.h264", "03_encrypted_s3_iframe.png"),
        ("03_decrypted_s3.h264", "03_decrypted_s3_iframe.png"),
    ]
    
    print(f"\n🎬 Extracting First I-Frames")
    print(f"   Source dir: {sample_dir}")
    print(f"   Output format: PNG (320px width)")
    print()
    
    success_count = 0
    for input_name, output_name in files:
        input_file = sample_dir / input_name
        output_file = sample_dir / output_name
        
        if not input_file.exists():
            print(f"  ❌ {input_name} not found")
            continue
        
        if extract_first_iframe(input_file, output_file):
            success_count += 1
    
    print(f"\n✅ Extracted {success_count}/{len(files)} first I-frames")
    print(f"   Location: {sample_dir}/")
    print()
    
    # List all files
    print("📁 Contents of sample_frames/:")
    for f in sorted(sample_dir.iterdir()):
        size_mb = f.stat().st_size / (1024*1024)
        print(f"   {f.name:40} {size_mb:8.2f} MB")


if __name__ == "__main__":
    main()
