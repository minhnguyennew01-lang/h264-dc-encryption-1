# DC Extraction Pipeline - Quick Guide

## What We Built

Three complete H.264 encryption pipelines with **integrated DC coefficient extraction**:

1. **`pipeline_encrypt_strategy1_with_dc`** - Encrypt ALL frames
2. **`pipeline_encrypt_strategy2_with_dc`** - Encrypt I-frames ONLY  
3. **`pipeline_encrypt_strategy3_with_dc`** - Encrypt I+P frames

Each pipeline:
- ✅ Extracts DC BEFORE encryption → `{output}.h264.dc_before.txt`
- ✅ Performs encryption according to strategy
- ✅ Extracts DC AFTER encryption → `{output}.h264.dc_after.txt`
- ✅ Saves encrypted video → `{output}.h264`
- ✅ Saves metadata → `{output}.h264.meta`

---

## How to Use

### Compile
```bash
cd /home/minh/Documents/NCKH20262/bitstream_impl

# Strategy 1 (All frames)
g++ -std=c++17 -O2 -o pipeline_encrypt_s1_dc pipeline_encrypt_strategy1_with_dc.cpp encryption.cpp

# Strategy 2 (I-frames only)
g++ -std=c++17 -O2 -o pipeline_encrypt_s2_dc pipeline_encrypt_strategy2_with_dc.cpp encryption.cpp

# Strategy 3 (I + P frames)
g++ -std=c++17 -O2 -o pipeline_encrypt_s3_dc pipeline_encrypt_strategy3_with_dc.cpp encryption.cpp
```

### Run
```bash
# Strategy 1
./pipeline_encrypt_s1_dc input.h264 output_s1.h264 mykey123

# Strategy 2  
./pipeline_encrypt_s2_dc input.h264 output_s2.h264 mykey123

# Strategy 3
./pipeline_encrypt_s3_dc input.h264 output_s3.h264 mykey123
```

### Output Files
Each run generates:
```
output_s1.h264                    # Encrypted video
output_s1.h264.meta              # NALU boundaries & metadata
output_s1.h264.dc_before.txt     # DC before encryption
output_s1.h264.dc_after.txt      # DC after encryption
```

---

## DC Output Format

### File Header
```
# DC Coefficients - BEFORE/AFTER ENCRYPTION
# Format: NALU_TYPE Y[16] Cb[4] Cr[4]
# Y (Luma): 16 DC values per macroblock
# Cb/Cr (Chroma): 4 DC values per macroblock (subsampled 4:2:0)
```

### Data Rows
```
NALU [Type:5] Y: -120-124  47  -1  -2 -10 -82  -4 -53  43 116 126-107  46  29  89 | Cb:  123 -77  81 -14 | Cr:  -24  73 114  -3
NALU [Type:1] Y: -102  34 108  66 127  -3 -15   3   3   3   3   3   3   3   3   3 | Cb:    3   3   3   3 | Cr:    3   3   3   3
```

### Interpretation
- **Type:5** = I-frame (Independent frame, keyframe)
- **Type:1** = P/B-frame (Predicted/Bidirectional frame)
- **Y:** = Luma component (16 brightness DC values)
- **Cb:** = Blue chroma component (4 color DC values)
- **Cr:** = Red chroma component (4 color DC values)

---

## Quick Comparison

| Metric | Strategy 1 | Strategy 2 | Strategy 3 |
|--------|-----------|-----------|-----------|
| **Frames Encrypted** | All (98.39%) | I-frames only (0.80%) | I+P (98.39%) |
| **DC Before** | 456,912 | 456,912 | 456,912 |
| **DC After** | 456,912 | 456,912 | 456,960 |
| **Encryption Level** | 🟢 Maximum | 🟠 Minimal | 🟢 Maximum |
| **Use Case** | Highest security | Research only | Practical privacy |

---

## Key Files in This Pipeline

| File | Purpose |
|------|---------|
| `dc_extractor.h` | Reusable DC extraction utility |
| `pipeline_encrypt_strategy1_with_dc.cpp` | S1 pipeline (all frames) |
| `pipeline_encrypt_strategy2_with_dc.cpp` | S2 pipeline (I-frames only) |
| `pipeline_encrypt_strategy3_with_dc.cpp` | S3 pipeline (I+P frames) |
| `encryption.h/cpp` | Core encryption algorithm (PLCM + Arnold Cat Map + XOR) |

---

## What Changed vs Original

### Before (Separate DC Extraction)
1. Run encryption pipeline
2. Separately extract DC with another tool
3. No control over timing

### After (Integrated DC Extraction)
1. Extract DC BEFORE encryption (know original values)
2. Run encryption pipeline (same key, same algorithm)
3. Extract DC AFTER encryption (see randomized values)
4. Direct before/after comparison possible

**Benefits**:
- ✅ Synchronized extraction at exact encryption moments
- ✅ Perfect alignment for before/after comparison
- ✅ No separate tools needed
- ✅ Automatic file generation with consistent naming
- ✅ Y/Cb/Cr components properly separated

---

## Analysis

### Visualize Changes
```bash
diff output_s1.h264.dc_before.txt output_s1.h264.dc_after.txt | head -20
```

### Count DC Coefficients
```bash
grep "NALU" output_s1.h264.dc_before.txt | wc -l
```

### Find Specific NALU Types
```bash
# Find all I-frames (Type:5)
grep "Type:5" output_s1.h264.dc_before.txt | head -5

# Find all P/B-frames (Type:1)
grep "Type:1" output_s1.h264.dc_before.txt | head -5
```

---

## See Also
- `STRATEGY_COMPARISON.md` - Detailed strategy comparison
- `DC_SAMPLES_ALL_STRATEGIES.txt` - Real DC value examples from all three strategies
