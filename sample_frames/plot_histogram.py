#!/usr/bin/env python3
"""
Plot histogram comparison: Original vs Encrypted (S1)
"""
import cv2
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

# Load images
sample_dir = Path(__file__).parent
original_img = cv2.imread(str(sample_dir / "00_original_iframe.png"))
encrypted_img = cv2.imread(str(sample_dir / "01_encrypted_s1_iframe.png"))

# Convert BGR to RGB for matplotlib
original_rgb = cv2.cvtColor(original_img, cv2.COLOR_BGR2RGB)
encrypted_rgb = cv2.cvtColor(encrypted_img, cv2.COLOR_BGR2RGB)

# Create figure with 4 subplots
fig = plt.figure(figsize=(16, 10))

# Row 1: Original images
ax1 = plt.subplot(2, 3, 1)
ax1.imshow(original_rgb)
ax1.set_title("Original I-Frame #12", fontsize=12, fontweight='bold')
ax1.axis('off')

ax2 = plt.subplot(2, 3, 2)
ax2.imshow(encrypted_rgb)
ax2.set_title("Encrypted S1 I-Frame #12", fontsize=12, fontweight='bold')
ax2.axis('off')

# Row 1: Histogram comparison
ax3 = plt.subplot(2, 3, 3)
colors = ('r', 'g', 'b')
for i, color in enumerate(colors):
    hist_orig = cv2.calcHist([original_img], [i], None, [256], [0, 256])
    ax3.plot(hist_orig, color=color, label=f'{color.upper()} (Original)', linewidth=2)

ax3.set_xlabel("Pixel Intensity")
ax3.set_ylabel("Frequency")
ax3.set_title("Original Histogram (RGB)", fontsize=12, fontweight='bold')
ax3.legend()
ax3.grid(True, alpha=0.3)
ax3.set_xlim([0, 255])

# Row 2: Encrypted histograms
ax4 = plt.subplot(2, 3, 4)
for i, color in enumerate(colors):
    hist_enc = cv2.calcHist([encrypted_img], [i], None, [256], [0, 256])
    ax4.plot(hist_enc, color=color, label=f'{color.upper()} (Encrypted)', linewidth=2)

ax4.set_xlabel("Pixel Intensity")
ax4.set_ylabel("Frequency")
ax4.set_title("Encrypted Histogram (RGB)", fontsize=12, fontweight='bold')
ax4.legend()
ax4.grid(True, alpha=0.3)
ax4.set_xlim([0, 255])

# Row 2: Side-by-side comparison
ax5 = plt.subplot(2, 3, 5)
hist_orig_r = cv2.calcHist([original_img], [2], None, [256], [0, 256]).flatten()
hist_enc_r = cv2.calcHist([encrypted_img], [2], None, [256], [0, 256]).flatten()
x = np.arange(256)
width = 0.35
ax5.bar(x[::8], hist_orig_r[::8], width, label='Original', alpha=0.7, color='blue')
ax5.bar(x[::8] + width, hist_enc_r[::8], width, label='Encrypted S1', alpha=0.7, color='red')
ax5.set_xlabel("Pixel Intensity")
ax5.set_ylabel("Frequency")
ax5.set_title("Red Channel Histogram Comparison", fontsize=12, fontweight='bold')
ax5.legend()
ax5.grid(True, alpha=0.3, axis='y')

# Row 2: Grayscale comparison
ax6 = plt.subplot(2, 3, 6)
orig_gray = cv2.cvtColor(original_img, cv2.COLOR_BGR2GRAY)
enc_gray = cv2.cvtColor(encrypted_img, cv2.COLOR_BGR2GRAY)
hist_orig_gray = cv2.calcHist([orig_gray], [0], None, [256], [0, 256]).flatten()
hist_enc_gray = cv2.calcHist([enc_gray], [0], None, [256], [0, 256]).flatten()

ax6.plot(hist_orig_gray, color='green', label='Original', linewidth=2, marker='o', markevery=20)
ax6.plot(hist_enc_gray, color='red', label='Encrypted S1', linewidth=2, marker='s', markevery=20)
ax6.set_xlabel("Pixel Intensity (0-255)")
ax6.set_ylabel("Frequency")
ax6.set_title("Grayscale Histogram Comparison", fontsize=12, fontweight='bold')
ax6.legend()
ax6.grid(True, alpha=0.3)
ax6.set_xlim([0, 255])

plt.tight_layout()
output_path = sample_dir / "histogram_comparison.png"
plt.savefig(output_path, dpi=150, bbox_inches='tight')
print(f"✅ Histogram saved: {output_path}")

# Print statistics
print("\n📊 HISTOGRAM STATISTICS\n")
print("=" * 70)
print(f"{'Metric':<30} {'Original':<20} {'Encrypted S1':<20}")
print("=" * 70)

# Original stats
orig_mean = np.mean(orig_gray)
orig_std = np.std(orig_gray)
orig_min = np.min(orig_gray)
orig_max = np.max(orig_gray)

# Encrypted stats
enc_mean = np.mean(enc_gray)
enc_std = np.std(enc_gray)
enc_min = np.min(enc_gray)
enc_max = np.max(enc_gray)

print(f"{'Mean Intensity':<30} {orig_mean:<20.2f} {enc_mean:<20.2f}")
print(f"{'Std Deviation':<30} {orig_std:<20.2f} {enc_std:<20.2f}")
print(f"{'Min Intensity':<30} {orig_min:<20.0f} {enc_min:<20.0f}")
print(f"{'Max Intensity':<30} {orig_max:<20.0f} {enc_max:<20.0f}")
print("=" * 70)

# Entropy calculation
def calculate_entropy(hist):
    hist = hist.astype(float) / hist.sum()
    entropy = -np.sum(hist * np.log2(hist + 1e-10))
    return entropy

orig_entropy = calculate_entropy(hist_orig_gray)
enc_entropy = calculate_entropy(hist_enc_gray)

print(f"\n🔒 ENCRYPTION QUALITY METRICS\n")
print(f"Original Entropy:        {orig_entropy:.4f} bits")
print(f"Encrypted Entropy:       {enc_entropy:.4f} bits")
print(f"Entropy Increase:        {((enc_entropy - orig_entropy) / orig_entropy * 100):.2f}%")
print(f"\n💡 Perfect encryption should have entropy close to 8.0 bits (max)")
print(f"   Current encrypted entropy: {enc_entropy:.4f} bits (uniform distribution)")

plt.show()
