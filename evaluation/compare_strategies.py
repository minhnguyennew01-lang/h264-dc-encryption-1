#!/usr/bin/env python3
"""
Generate comparison table for all three strategies
"""
import json
from pathlib import Path

try:
    from tabulate import tabulate
    HAS_TABULATE = True
except ImportError:
    HAS_TABULATE = False
    def tabulate(rows, headers, tablefmt="grid"):
        """Simple fallback table formatter"""
        col_widths = [max(len(str(h)), max(len(str(r[i])) for r in rows)) for i, h in enumerate(headers)]
        
        # Header
        header_line = " | ".join(h.ljust(w) for h, w in zip(headers, col_widths))
        sep_line = "-+-".join("-" * w for w in col_widths)
        
        lines = [sep_line, header_line, sep_line]
        
        # Rows
        for row in rows:
            lines.append(" | ".join(str(v).ljust(w) for v, w in zip(row, col_widths)))
        
        lines.append(sep_line)
        return "\n".join(lines)

EVAL_DIR = Path(__file__).parent
RESULT_FILES = {
    "Strategy 1": EVAL_DIR / "result_strategy1.json",
    "Strategy 2": EVAL_DIR / "result_strategy2.json",
    "Strategy 3": EVAL_DIR / "result_strategy3.json",
}


def load_result(filepath):
    """Load result JSON file"""
    if not filepath.exists():
        return None
    with open(filepath, 'r') as f:
        return json.load(f)


def format_value(val, suffix="", decimals=2):
    """Format a value for display"""
    if val is None:
        return "N/A"
    if isinstance(val, float):
        if val == float('inf'):
            return "∞"
        return f"{val:.{decimals}f}{suffix}"
    return f"{val}{suffix}"


def main():
    results = {}
    for strategy, filepath in RESULT_FILES.items():
        results[strategy] = load_result(filepath)
    
    # Prepare table data
    headers = [
        "Metric",
        "Strategy 1",
        "Strategy 2",
        "Strategy 3",
    ]
    
    rows = []
    
    # File size metrics
    rows.append(["Original size", 
                 format_value(results["Strategy 1"]["original"]["size_bytes"], " bytes", decimals=0),
                 "same", "same"])
    
    rows.append(["Frame count", 
                 format_value(results["Strategy 1"].get("frame_count")),
                 "same", "same"])
    
    # Encryption timing
    rows.append(["Encryption time", 
                 format_value(results["Strategy 1"].get("encryption_time_ms"), " ms"),
                 format_value(results["Strategy 2"].get("encryption_time_ms"), " ms"),
                 format_value(results["Strategy 3"].get("encryption_time_ms"), " ms")])
    
    rows.append(["Time per frame", 
                 format_value(results["Strategy 1"].get("encryption_time_per_frame_ms"), " ms/frame", decimals=3),
                 format_value(results["Strategy 2"].get("encryption_time_per_frame_ms"), " ms/frame", decimals=3),
                 format_value(results["Strategy 3"].get("encryption_time_per_frame_ms"), " ms/frame", decimals=3)])
    
    # Encrypted file metrics
    rows.append(["", "", "", ""])  # Separator
    rows.append(["[ENCRYPTED FILE]", "", "", ""])
    rows.append(["Size change %", 
                 format_value(results["Strategy 1"].get("encrypted_size_change_pct"), "%"),
                 format_value(results["Strategy 2"].get("encrypted_size_change_pct"), "%"),
                 format_value(results["Strategy 3"].get("encrypted_size_change_pct"), "%")])
    
    rows.append(["SSIM (orig→encrypted)", 
                 format_value(results["Strategy 1"].get("ssim_original_encrypted"), "", decimals=4),
                 format_value(results["Strategy 2"].get("ssim_original_encrypted"), "", decimals=4),
                 format_value(results["Strategy 3"].get("ssim_original_encrypted"), "", decimals=4)])
    
    rows.append(["PSNR (orig→encrypted)", 
                 format_value(results["Strategy 1"].get("psnr_original_encrypted"), " dB"),
                 format_value(results["Strategy 2"].get("psnr_original_encrypted"), " dB"),
                 format_value(results["Strategy 3"].get("psnr_original_encrypted"), " dB")])
    
    rows.append(["STRRED (orig→encrypted)", 
                 format_value(results["Strategy 1"].get("strred_original_encrypted"), "", decimals=3),
                 format_value(results["Strategy 2"].get("strred_original_encrypted"), "", decimals=3),
                 format_value(results["Strategy 3"].get("strred_original_encrypted"), "", decimals=3)])
    
    # Decrypted file metrics
    rows.append(["", "", "", ""])  # Separator
    rows.append(["[DECRYPTED FILE]", "", "", ""])
    rows.append(["Size change %", 
                 format_value(results["Strategy 1"].get("decrypted_size_change_pct"), "%"),
                 format_value(results["Strategy 2"].get("decrypted_size_change_pct"), "%"),
                 format_value(results["Strategy 3"].get("decrypted_size_change_pct"), "%")])
    
    rows.append(["SSIM (orig→decrypted)", 
                 format_value(results["Strategy 1"].get("ssim_original_decrypted"), "", decimals=4),
                 format_value(results["Strategy 2"].get("ssim_original_decrypted"), "", decimals=4),
                 format_value(results["Strategy 3"].get("ssim_original_decrypted"), "", decimals=4)])
    
    rows.append(["PSNR (orig→decrypted)", 
                 format_value(results["Strategy 1"].get("psnr_original_decrypted"), " dB"),
                 format_value(results["Strategy 2"].get("psnr_original_decrypted"), " dB"),
                 format_value(results["Strategy 3"].get("psnr_original_decrypted"), " dB")])
    
    rows.append(["STRRED (orig→decrypted)", 
                 format_value(results["Strategy 1"].get("strred_original_decrypted"), "", decimals=3),
                 format_value(results["Strategy 2"].get("strred_original_decrypted"), "", decimals=3),
                 format_value(results["Strategy 3"].get("strred_original_decrypted"), "", decimals=3)])
    
    # Print table
    print("\n" + "="*100)
    print("H.264 CROSS-NALU ENCRYPTION - COMPREHENSIVE EVALUATION")
    print("="*100 + "\n")
    
    print(tabulate(rows, headers=headers, tablefmt="grid"))
    
    print("\n" + "="*100)
    print("LEGEND:")
    print("  SSIM: Structural Similarity Index (1.0 = identical, 0 = completely different)")
    print("  PSNR: Peak Signal-to-Noise Ratio (higher = better, ∞ = perfect recovery)")
    print("  STRRED: Spatio-Temporal Reduced Reference (lower = better quality)")
    print("="*100 + "\n")


if __name__ == "__main__":
    try:
        main()
    except ImportError:
        print("ERROR: tabulate module not found. Install with: pip install tabulate")
        print("\nFallback: Display JSON results...")
        for strategy, filepath in RESULT_FILES.items():
            result = load_result(filepath)
            if result:
                print(f"\n{strategy}:")
                print(json.dumps(result, indent=2))
