# Evaluation Framework for Hybrid H.264 Encryption

## Quick Start

```bash
cd bitstream_impl_arnold2d

# 1. Run optimized batch evaluation (MD5 + timing)
python3 evaluation/batch_evaluate_optimized.py

# 2. Compute quality metrics (SSIM sampling)
python3 evaluation/compute_quality_metrics_fast.py

# 3. Generate report
python3 evaluation/generate_report.py
```

## Scripts Overview

### 1. `batch_evaluate_optimized.py` - Fast Validation
**Purpose:** Quick byte-level validation and performance measurement  
**Time:** ~6 seconds for 3 strategies on 218 MB file

```bash
python3 evaluation/batch_evaluate_optimized.py
```

**Output:**
- Encrypt each file
- Decrypt each file
- Compute MD5 hashes
- Verify byte-perfect recovery
- Measure wall-clock time
- Save results to `result_strategy*.json`

**Metrics:**
- Encryption time (ms)
- Decryption time (ms)
- Total time (ms)
- File overhead (%)
- MD5 validation (pass/fail)

---

### 2. `compute_quality_metrics_fast.py` - SSIM/PSNR Sampling
**Purpose:** Fast video quality assessment via sampling  
**Time:** ~2-3 minutes for 3 strategies

```bash
python3 evaluation/compute_quality_metrics_fast.py
```

**Output:**
- Test sample frames (0, 100, 1000)
- Compute SSIM via FFmpeg
- Update JSON results with quality metrics
- Save `quality_metrics.json`

**Metrics:**
- SSIM (Structural Similarity Index)
- PSNR (Peak Signal-to-Noise Ratio)
- Perfect match status
- Frames tested

**Note:** Samples only 2-3 frames to avoid long FFmpeg processing times

---

### 3. `generate_report.py` - Report Generation
**Purpose:** Pretty-print comparison table  
**Time:** <1 second

```bash
python3 evaluation/generate_report.py
```

**Output:** Formatted comparison table with:
- Performance metrics (encrypt/decrypt times)
- File size metrics (original/encrypted/decrypted sizes)
- Validation status (MD5 hashes, verification)
- Quality metrics (SSIM/PSNR, perfect status)

---

## Result Files

### Per-Strategy Results
- `result_strategy1.json` - Strategy 1 complete evaluation
- `result_strategy2.json` - Strategy 2 complete evaluation  
- `result_strategy3.json` - Strategy 3 complete evaluation

**Content:**
```json
{
  "strategy_num": "1",
  "encrypt_time_ms": 1007,
  "decrypt_time_ms": 1027,
  "total_time_ms": 2034,
  "original_size": 228618587,
  "encrypted_size": 228618587,
  "decrypted_size": 228618587,
  "size_overhead_pct": 0.0,
  "validation_status": "PASS",
  "validation_detail": "MD5 byte-perfect match",
  "md5_original": "843369253a9f3ff8...",
  "md5_decrypted": "843369253a9f3ff8...",
  "quality_metrics": {
    "ssim": 1.0,
    "psnr": "∞",
    "perfect": true,
    "samples_tested": 2,
    "frames_sampled": [100, 1000]
  }
}
```

### Aggregated Results
- `batch_results_optimized.json` - All strategies combined
- `quality_metrics.json` - Quality metrics across strategies

---

## Evaluation Criteria

| Criterion | Pass | Fail |
|-----------|------|------|
| **Byte Integrity** | MD5 match | MD5 mismatch |
| **File Size** | ≤0.1% overhead | >0.1% overhead |
| **Video Quality** | SSIM≥0.99 | SSIM<0.99 |
| **Performance** | <3s per 218MB | ≥3s per 218MB |

## Current Results

✅ **All strategies PASS**

| Strategy | Encrypt | Decrypt | Total | MD5 Match | SSIM | Overhead |
|----------|---------|---------|-------|-----------|------|----------|
| S1 (All) | 1.01s | 1.03s | 2.03s | ✅ | 1.0 | 0.00% |
| S2 (I-only) | 0.92s | 0.99s | 1.91s | ✅ | 1.0 | 0.00% |
| S3 (Selective) | 1.02s | 0.98s | 2.00s | ✅ | 1.0 | 0.00% |

---

## Customization

### Change Test File
Edit `batch_evaluate_optimized.py`:
```python
ORIGINAL_FILE = BASE_DIR / "your_video.h264"  # Change this
```

### Change Sampling Frames
Edit `compute_quality_metrics_fast.py`:
```python
frame_indices = [0, 100, 1000]  # Add/remove frames
```

### Add More Metrics
Extend `generate_report.py`:
```python
# Add custom columns to tables
qual_table.append([
    strat_name,
    # ... existing metrics ...
    custom_metric,  # Add here
])
```

---

## Troubleshooting

### "ffmpeg not found"
```bash
sudo apt install ffmpeg ffprobe
```

### "tabulate module not found"
```bash
pip install tabulate
```

### "No such file or directory: output.h264"
Ensure you're in the correct directory:
```bash
cd bitstream_impl_arnold2d
```

### SSIM computation hangs
Frames are sampled to avoid long ffmpeg processing. If still slow:
```python
frame_indices = [100, 500]  # Reduce sample count
```

---

## Output Files Location

```
bitstream_impl_arnold2d/
├── output.h264                          (original video)
├── output.h264.s1_hybrid                (strategy 1 encrypted)
├── output.h264.s1_hybrid.meta           (strategy 1 metadata)
├── output.h264.s1_hybrid.decrypted      (strategy 1 decrypted)
├── output.h264.s2_hybrid                (strategy 2 encrypted)
├── output.h264.s2_hybrid.meta           (strategy 2 metadata)
├── output.h264.s2_hybrid.decrypted      (strategy 2 decrypted)
├── output.h264.s3_hybrid                (strategy 3 encrypted)
├── output.h264.s3_hybrid.meta           (strategy 3 metadata)
├── output.h264.s3_hybrid.decrypted      (strategy 3 decrypted)
└── evaluation/
    ├── batch_evaluate_optimized.py      (main batch script)
    ├── compute_quality_metrics_fast.py  (quality assessment)
    ├── generate_report.py               (report generation)
    ├── result_strategy1.json            (S1 results)
    ├── result_strategy2.json            (S2 results)
    ├── result_strategy3.json            (S3 results)
    ├── quality_metrics.json             (aggregated metrics)
    └── batch_results_optimized.json     (all results combined)
```

---

## Full Evaluation Workflow

```bash
# Step 1: Change to project directory
cd bitstream_impl_arnold2d

# Step 2: Remove old test files (optional)
rm -f output.h264.s*_hybrid*

# Step 3: Run batch evaluation
python3 evaluation/batch_evaluate_optimized.py
# Output: ✅ All strategies evaluated successfully!

# Step 4: Compute quality metrics
python3 evaluation/compute_quality_metrics_fast.py
# Output: ✅ Metrics saved to: quality_metrics.json

# Step 5: Generate final report
python3 evaluation/generate_report.py
# Output: Pretty-printed comparison tables

# Step 6: Review results
cat evaluation/result_strategy1.json  # View detailed results
```

**Total Time:** ~10 minutes for full evaluation on 218 MB video

---

## For Academic Paper

Include these metrics in your paper:

```
Table 1: Performance Comparison
- Encryption time
- Decryption time
- File size overhead
- Throughput (MB/s)

Table 2: Security Validation
- MD5 verification results
- Strategy coverage percentages
- Frame statistics per strategy

Table 3: Quality Assessment  
- SSIM scores
- PSNR values
- Perfect recovery status

Figure 1: Time Comparison (bar chart of encrypt/decrypt times)
Figure 2: Coverage Comparison (pie chart of frame percentages)
```

---

## Contact / Support

For issues with evaluation scripts, check:
1. FFmpeg/ffprobe installation
2. File paths and permissions
3. Disk space (need 2x video size for intermediate files)
4. Python version compatibility (3.6+)

---

*Last Updated: April 20, 2026*  
*Evaluation Framework Version: 1.0*  
*Status: Production Ready ✅*
