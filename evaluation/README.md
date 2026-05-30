# Evaluation Scripts for H264 Cross-NALU Encryption

This folder contains Python tooling to evaluate the encryption results of the H.264 Cross-NALU project.

## Scripts Available

### 1. `evaluate.py` (Basic Evaluation)
Evaluates a single pair of encrypted/decrypted files with basic metrics.

**Metrics:**
- File size and change percentage
- SSIM between original and decrypted files
- PSNR between original and decrypted files

**Usage:**
```bash
python3 evaluate.py --original ../output.h264 --encrypted ../encrypted_s1.h264 --decrypted ../decrypted_s1.h264 --strategy "Strategy 1"
```

### 2. `evaluate_enhanced.py` (Enhanced Evaluation)
Evaluates a single pair with additional metrics including STRRED and encryption timing.

**Metrics:**
- File size and change percentage
- SSIM (original→encrypted and original→decrypted)
- PSNR (original→encrypted and original→decrypted)
- STRRED (original→encrypted and original→decrypted)
- Encryption time and per-frame timing

**Usage:**
```bash
python3 evaluate_enhanced.py --original ../output.h264 --encrypted ../encrypted_s1.h264 --decrypted ../decrypted_s1.h264 --strategy "Strategy 1" --encryption-time-ms 1319.31 --output-json result_s1.json
```

### 3. `batch_evaluate.py` (Batch Processing)
Processes all three strategies in one go, with automatic encryption/decryption and timing.

**What it does:**
1. Encrypts and decrypts all three strategies
2. Measures encryption/decryption time
3. Evaluates each strategy
4. Generates individual JSON results

**Usage:**
```bash
python3 batch_evaluate.py
```

### 4. `compare_strategies.py` (Comparison Table)
Generates a comprehensive comparison table of all three strategies.

**Usage:**
```bash
python3 compare_strategies.py
```

## Metrics Explanation

| Metric | Explanation |
|--------|-------------|
| **SSIM** | Structural Similarity Index (0-1 scale, 1.0 = identical) |
| **PSNR** | Peak Signal-to-Noise Ratio in dB (higher = better, ∞ = perfect) |
| **STRRED** | Spatio-Temporal Reduced Reference (lower = better quality) |
| **File size change %** | Percentage change in file size after encryption |
| **Encryption time** | Total time to encrypt in milliseconds |
| **Time per frame** | Average encryption time per frame in ms/frame |

## Dependencies

- Python 3
- `ffmpeg` and `ffprobe` (must be in PATH)
- `tabulate` (optional, for better table formatting)

## Typical Workflow

1. **Quick single evaluation:**
   ```bash
   python3 evaluate.py --original ../output.h264 --encrypted ../encrypted_s1.h264 --decrypted ../decrypted_s1.h264 --strategy "Strategy 1"
   ```

2. **Full batch evaluation with timing:**
   ```bash
   python3 batch_evaluate.py
   ```

3. **View comparison:**
   ```bash
   python3 compare_strategies.py
   ```

## Output Files

- `result_1.json` - Strategy 1 detailed results
- `result_2.json` - Strategy 2 detailed results
- `result_3.json` - Strategy 3 detailed results
- `batch_results.log` - Complete batch run log

## Notes

- **SSIM/PSNR (encrypted files):** Will show as "N/A" if ffmpeg cannot decode the encrypted bitstream (expected behavior)
- **STRRED:** Currently returns None due to ffmpeg filter limitations with H.264 files
- **Perfect Recovery:** SSIM=1.0 and PSNR=∞ for decrypted files indicate 100% byte-perfect recovery

