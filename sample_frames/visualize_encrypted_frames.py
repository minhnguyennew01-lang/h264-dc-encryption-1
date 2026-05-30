#!/usr/bin/env python3
"""
Visualize encrypted frames extracted từ NALU payload
So sánh các cách khác nhau để extract frame
"""

import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path
import re

def read_pgm(filename):
    """Đọc file PGM (Portable Graymap format)"""
    with open(filename, 'rb') as f:
        # Skip header
        lines = []
        while True:
            line = f.readline().decode('ascii').strip()
            if line and not line.startswith('#'):
                lines.append(line)
            if len(lines) >= 3:  # P5, width height, maxval
                break
        
        width, height = map(int, lines[1].split())
        pixels = np.frombuffer(f.read(), dtype=np.uint8)
        pixels = pixels.reshape((height, width))
        return pixels, width, height

def main():
    sample_dir = Path(__file__).parent
    
    # Tìm tất cả file encrypted
    raw_files = list(sample_dir.glob("*_raw_encrypted.pgm"))
    
    if not raw_files:
        print("❌ Không tìm thấy *_raw_encrypted.pgm files")
        return
    
    # Tạo figure cho mỗi file
    for pgm_file in sorted(raw_files):
        try:
            print(f"\n📊 Visualizing: {pgm_file.name}")
            
            pixels, width, height = read_pgm(pgm_file)
            
            # Thông tin cơ bản
            print(f"   Size: {width}x{height}")
            print(f"   Min pixel: {pixels.min()}, Max pixel: {pixels.max()}")
            print(f"   Mean: {pixels.mean():.2f}, Std: {pixels.std():.2f}")
            
            # Tính entropy
            hist, _ = np.histogram(pixels, bins=256, range=(0, 256))
            prob = hist[hist > 0] / pixels.size
            entropy = -np.sum(prob * np.log2(prob))
            print(f"   Entropy: {entropy:.4f} bits/pixel")
            
            # Tạo figure
            fig, axes = plt.subplots(2, 2, figsize=(12, 10))
            fig.suptitle(f"Encrypted Frame Analysis - {pgm_file.stem}", fontsize=14, fontweight='bold')
            
            # 1. Ảnh gốc
            axes[0, 0].imshow(pixels, cmap='gray')
            axes[0, 0].set_title('Raw Encrypted Frame')
            axes[0, 0].axis('off')
            
            # 2. Histogram
            axes[0, 1].hist(pixels.flatten(), bins=256, range=(0, 256), color='blue', alpha=0.7)
            axes[0, 1].set_title('Pixel Value Distribution (Histogram)')
            axes[0, 1].set_xlabel('Pixel Value (0-255)')
            axes[0, 1].set_ylabel('Frequency')
            axes[0, 1].grid(True, alpha=0.3)
            
            # 3. Cumulative distribution
            cum_hist = np.cumsum(hist) / pixels.size
            axes[1, 0].plot(cum_hist, linewidth=2, color='green')
            axes[1, 0].set_title('Cumulative Distribution')
            axes[1, 0].set_xlabel('Pixel Value')
            axes[1, 0].set_ylabel('Cumulative Probability')
            axes[1, 0].grid(True, alpha=0.3)
            
            # 4. Thống kê
            stats_text = f"""
Encrypted Frame Statistics:

Size: {width} × {height} pixels
Total pixels: {width * height:,}

Value Range:
  Min: {pixels.min()}
  Max: {pixels.max()}
  Mean: {pixels.mean():.2f}
  Std Dev: {pixels.std():.2f}

Distribution:
  Entropy: {entropy:.4f} bits
  Unique values: {np.unique(pixels).size}/256
  
Quality Metrics:
  Randomness: {'EXCELLENT' if entropy > 7.9 else 'GOOD' if entropy > 7 else 'POOR'}
  Distribution: {'UNIFORM' if pixels.std() > 70 else 'SKEWED'}
            """
            axes[1, 1].text(0.1, 0.5, stats_text, fontsize=10, family='monospace',
                           verticalalignment='center', bbox=dict(boxstyle='round', 
                           facecolor='wheat', alpha=0.5))
            axes[1, 1].axis('off')
            
            # Lưu figure
            output_fig = pgm_file.with_suffix('.png')
            plt.tight_layout()
            plt.savefig(output_fig, dpi=150, bbox_inches='tight')
            print(f"   ✅ Saved: {output_fig.name}")
            plt.close()
            
        except Exception as e:
            print(f"   ❌ Lỗi: {e}")
            import traceback
            traceback.print_exc()

if __name__ == "__main__":
    main()
    print("\n✅ Hoàn thành!")
