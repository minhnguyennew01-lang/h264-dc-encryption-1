# H.264 Video Encryption - Complete Project Index

**Date:** April 18, 2026  
**Location:** `/home/minh/Documents/NCKH20262/bitstream_impl/`

## 🎯 Project Overview

This project implements 3 H.264 video encryption strategies using PLCM (Piecewise Linear Chaotic Map) + Arnold Cat Map + XOR diffusion, with **byte-perfect decryption** achieved through NALU metadata preservation.

---

## 📂 Directory Structure

```
bitstream_impl/
├── Core Encryption/Decryption Programs
│   ├── pipeline_encrypt_strategy1_preserve          (Executable)
│   ├── pipeline_encrypt_strategy1_preserve.cpp      (Source)
│   ├── pipeline_encrypt_strategy2_preserve          (Executable)
│   ├── pipeline_encrypt_strategy2_preserve.cpp      (Source)
│   ├── pipeline_encrypt_strategy3_preserve          (Executable)
│   ├── pipeline_encrypt_strategy3_preserve.cpp      (Source)
│   ├── pipeline_decrypt_preserve                    (Executable)
│   └── pipeline_decrypt_preserve_perfect.cpp        (Source)
│
├── Supporting Libraries
│   ├── encryption.h / encryption.cpp                (PLCM + Arnold + XOR)
│   ├── rbsp.h / rbsp.cpp                            (Raw Byte Sequence Payload)
│   ├── bitio.h / bitio.cpp                          (Bit-level I/O)
│   ├── cavlc.h / cavlc_v2.cpp                       (CAVLC decoding)
│   └── analyze_video.cpp                            (Video analysis)
│
├── DC Coefficient Analysis
│   ├── extract_dc_all_strategies                    (Executable)
│   ├── extract_dc_all_strategies.cpp                (Source - Overall DC extraction)
│   ├── extract_dc_by_nalu                           (Executable)
│   └── extract_dc_by_nalu.cpp                       (Source - Per-NALU Y/Cb/Cr)
│
├── Build & Test
│   ├── Makefile                                     (Build configuration)
│   ├── test_and_save_results.sh                     (Full test script)
│   └── results/                                     (Test output directory)
│
├── Analysis Reports
│   ├── DC_EXTRACTION_REPORT.txt                     (Detailed DC analysis)
│   ├── DC_COMPARISON_SUMMARY.txt                    (Strategy comparison)
│   ├── DC_BY_NALU_ANALYSIS.txt                      (Y/Cb/Cr component analysis)
│   └── CLEANUP_SUMMARY.md                           (Cleanup documentation)
│
├── Data Files
│   ├── dc_original.txt                              (Overall DC - Original)
│   ├── dc_strategy1_encrypted.txt                   (Overall DC - S1)
│   ├── dc_strategy2_encrypted.txt                   (Overall DC - S2)
│   ├── dc_strategy3_encrypted.txt                   (Overall DC - S3)
│   ├── dc_original_by_nalu.txt                      (Per-NALU DC - Original)
│   ├── dc_strategy1_by_nalu.txt                     (Per-NALU DC - S1)
│   ├── dc_strategy2_by_nalu.txt                     (Per-NALU DC - S2)
│   └── dc_strategy3_by_nalu.txt                     (Per-NALU DC - S3)
│
└── Documentation
    ├── INDEX.md                                     (This file)
    ├── .gitignore                                   (Git ignore rules)
    └── Makefile                                     (See build instructions)
```

---

## 🚀 Quick Start

### Build All Programs
```bash
cd /home/minh/Documents/NCKH20262/bitstream_impl
make
```

### Encrypt Video (Strategy 2 Recommended)
```bash
./pipeline_encrypt_strategy2_preserve input.h264 encrypted.h264 mykey123
```

### Decrypt Video
```bash
./pipeline_decrypt_preserve encrypted.h264 original.h264 decrypted.h264 mykey123
```

### Run Full Test Suite
```bash
./test_and_save_results.sh
# Results saved to ./results/ directory
```

---

## 📋 3 Encryption Strategies

| Strategy | Coverage | File Type | Use Case | Speed |
|----------|----------|-----------|----------|-------|
| **S1: I/P/B** | 98.4% (19,038 NALUs) | `pipeline_encrypt_strategy1_preserve` | Maximum Security | Slow |
| **S2: I-frames** | 0.8% (155 NALUs) | `pipeline_encrypt_strategy2_preserve` | Streaming (⭐ RECOMMENDED) | Fast |
| **S3: I+P** | 98.4% (19,038 NALUs) | `pipeline_encrypt_strategy3_preserve` | Balanced | Medium |

### Strategy Details

**Strategy 1 - I/P/B Frame Encryption (98.4%)**
- Encrypts: I-frames + P-frames + B-frames
- Security: MAXIMUM (all content encrypted)
- Visual Impact: MAXIMUM (complete scrambling)
- File: `pipeline_encrypt_strategy1_preserve.cpp` (237 lines)

**Strategy 2 - I-Frame Only Encryption (0.8%)** ⭐ RECOMMENDED
- Encrypts: I-frames only (155 frames)
- Security: GOOD (key frames protected)
- Visual Impact: MINIMAL (99.2% frames unchanged)
- File: `pipeline_encrypt_strategy2_preserve.cpp` (188 lines)
- Use Case: Video streaming, real-time transmission

**Strategy 3 - I+P Frame Encryption (98.4%)**
- Encrypts: I-frames + P-frames (19,038 frames)
- Security: STRONG (all key frames)
- Visual Impact: HIGH (balanced approach)
- File: `pipeline_encrypt_strategy3_preserve.cpp` (207 lines)

---

## 🔐 Encryption Algorithm

### PLCM (Piecewise Linear Chaotic Map)
- **Parameters:** μ = 0.37, x₀ = 0.2, 1000 pre-iterations
- **Purpose:** Generate pseudo-random keystream
- **Class:** `KeystreamGenerator` in `encryption.h/cpp`

### Arnold Cat Map (Confusion)
- **Rounds:** 5 iterations per NALU
- **Purpose:** Cyclic shift for position confusion
- **Effect:** Scrambles byte positions

### XOR Diffusion
- **Method:** XOR with chaining (uses previous ciphertext)
- **Formula:** C[i] = P[i] ⊕ K[i] ⊕ C[i-1]
- **Purpose:** Diffuse changes across stream

---

## ✅ Verification Results

All 3 strategies achieve **BYTE-PERFECT DECRYPTION**:

| Strategy | Original Size | Encrypted Size | Decrypted Size | Match |
|----------|---------------|----------------|----------------|-------|
| Strategy 1 | 228,618,587 | 228,618,587 | 228,618,587 | ✓ YES |
| Strategy 2 | 228,618,587 | 228,618,587 | 228,618,587 | ✓ YES |
| Strategy 3 | 228,618,587 | 228,618,587 | 228,618,587 | ✓ YES |

Verified with: `cmp original.h264 decrypted.h264`

---

## 📊 DC Coefficient Analysis

### Files Generated

1. **Overall DC Analysis:**
   - `dc_original.txt` (3.3 MB) - 937,014 original DC values
   - `dc_strategy1_encrypted.txt` - 943,520 values (randomized)
   - `dc_strategy2_encrypted.txt` - 937,014 values (same count, S2 unencrypts P-frames)
   - `dc_strategy3_encrypted.txt` - 943,520 values (randomized)

2. **Per-NALU Component Analysis:**
   - `dc_original_by_nalu.txt` (3.7 MB) - Y[16] + Cb[4] + Cr[4] per NALU
   - `dc_strategy1_by_nalu.txt` - S1 per-NALU breakdown
   - `dc_strategy2_by_nalu.txt` - S2 per-NALU breakdown
   - `dc_strategy3_by_nalu.txt` - S3 per-NALU breakdown

### YUV 4:2:0 Component Distribution

Per NALU:
- **Y (Luma):** 16 DC coefficients (brightness, full resolution)
- **Cb (Chroma Blue):** 4 DC coefficients (color blue, 1/4 resolution)
- **Cr (Chroma Red):** 4 DC coefficients (color red, 1/4 resolution)

**Total per file:**
- Y: 302,128 coefficients
- Cb: 75,532 coefficients
- Cr: 75,532 coefficients
- **Total: 453,192 coefficients**

### Key Findings

**Strategy 1 & 3 Encryption Impact:**
- Original: Low entropy (values 0-10, highly compressible)
- Encrypted: High entropy (values -128 to +127, incompressible)
- All Y/Cb/Cr components completely randomized

**Strategy 2 Encryption Impact:**
- P-frame Y/Cb/Cr: UNCHANGED from original (99.2%)
- I-frame Y/Cb/Cr: RANDOMIZED (0.8%)
- Mixed distribution confirms selective encryption

---

## 🛠️ Build & Compilation

### Prerequisites
- C++11 compatible compiler (g++)
- Linux/Unix environment
- No external dependencies (self-contained)

### Build Commands
```bash
# Build all programs
make

# Build only encryption programs
make encrypt

# Build only decryption program
make decrypt

# Clean build artifacts
make clean

# Show help
make help
```

### Compilation Output
```
✓ pipeline_encrypt_strategy1_preserve  (75 KB)
✓ pipeline_encrypt_strategy2_preserve  (75 KB)
✓ pipeline_encrypt_strategy3_preserve  (75 KB)
✓ pipeline_decrypt_preserve            (76 KB)
```

---

## 📁 Test Results Directory

`results/` folder contains:
- `output_strategy1_encrypted.h264` (219 MB)
- `output_strategy1_decrypted.h264` (219 MB)
- `output_strategy1_encrypted.h264.meta` (171 KB)
- `output_strategy2_encrypted.h264` (219 MB)
- `output_strategy2_decrypted.h264` (219 MB)
- `output_strategy2_encrypted.h264.meta` (171 KB)
- `output_strategy3_encrypted.h264` (219 MB)
- `output_strategy3_decrypted.h264` (219 MB)
- `output_strategy3_encrypted.h264.meta` (171 KB)
- `README.txt` (12 KB)

**Total: 1.3 GB**

---

## 📖 Documentation Files

### Analysis Reports
1. **DC_EXTRACTION_REPORT.txt** (18 KB)
   - Comprehensive DC coefficient analysis
   - Entropy metrics per strategy
   - Visual impact assessment
   - Security implications

2. **DC_COMPARISON_SUMMARY.txt** (4.9 KB)
   - Quick comparison of 3 strategies
   - Key findings per strategy
   - Use case recommendations

3. **DC_BY_NALU_ANALYSIS.txt** (18 KB)
   - Per-NALU Y/Cb/Cr component breakdown
   - YUV 4:2:0 format explanation
   - Component-level security analysis
   - Visual degradation per component

4. **CLEANUP_SUMMARY.md** (3 KB)
   - Project cleanup documentation
   - Files deleted/kept rationale
   - Codebase optimization details

---

## 🎬 Example Usage Workflows

### Workflow 1: Encrypt and Decrypt
```bash
# Encrypt with Strategy 2
./pipeline_encrypt_strategy2_preserve video.h264 encrypted.h264 mykey

# Decrypt
./pipeline_decrypt_preserve encrypted.h264 video.h264 decrypted.h264 mykey

# Verify
cmp video.h264 decrypted.h264 && echo "MATCH" || echo "DIFFER"
```

### Workflow 2: Analyze DC Coefficients
```bash
# Extract overall DC values
./extract_dc_all_strategies video.h264 encrypted1.h264 encrypted2.h264 encrypted3.h264

# Extract per-NALU DC with Y/Cb/Cr separation
./extract_dc_by_nalu video.h264 encrypted1.h264 encrypted2.h264 encrypted3.h264

# Compare components
diff <(grep "Y\[" dc_original_by_nalu.txt) <(grep "Y\[" dc_strategy1_by_nalu.txt)
```

### Workflow 3: Full Test Suite
```bash
# Run complete test with all strategies
./test_and_save_results.sh

# Check results
ls -lh results/
cat results/README.txt
```

---

## 💾 File Inventory

### Executables (4 files)
- `extract_dc_all_strategies` - Overall DC extraction tool
- `extract_dc_by_nalu` - Per-NALU DC extraction tool
- `pipeline_encrypt_strategy1_preserve` - Strategy 1 encryption
- `pipeline_encrypt_strategy2_preserve` - Strategy 2 encryption
- `pipeline_encrypt_strategy3_preserve` - Strategy 3 encryption
- `pipeline_decrypt_preserve` - Universal decryption

### Source Code (14 files, ~2500 lines)
- `pipeline_encrypt_strategy1_preserve.cpp` (237 lines)
- `pipeline_encrypt_strategy2_preserve.cpp` (188 lines)
- `pipeline_encrypt_strategy3_preserve.cpp` (207 lines)
- `pipeline_decrypt_preserve_perfect.cpp` (153 lines)
- `extract_dc_all_strategies.cpp` (~150 lines)
- `extract_dc_by_nalu.cpp` (~250 lines)
- `encryption.cpp` (11 KB)
- `encryption.h` (1.3 KB)
- `rbsp.cpp/h`, `bitio.cpp/h`, `cavlc_v2.cpp/h`

### Data Files (12 files, ~15 MB)
- DC coefficient files (3.3 MB each)
- Per-NALU DC files (3.7 MB each)

### Documentation (4 files, ~45 KB)
- Analysis reports
- Cleanup summary
- This index file

---

## 🔍 Performance Metrics

### Encryption/Decryption Speed
| Strategy | Encrypt Time | Decrypt Time | Total |
|----------|--------------|--------------|-------|
| S1 | 11.16s | 9.28s | 20.44s |
| S2 | 3.90s | 3.54s | 7.44s |
| S3 | 11.31s | 9.27s | 20.58s |

**Test File:** 228.6 MB, 19,349 NALUs

### File Size Impact
- Original: 228,618,587 bytes
- Encrypted (all strategies): 228,618,587 bytes (unchanged)
- Metadata files: ~171 KB each

---

## ✨ Key Features

✓ **PLCM-based encryption** - Chaotic pseudo-random keystream  
✓ **Byte-perfect decryption** - NALU metadata preservation  
✓ **3 strategies** - Security/performance tradeoffs  
✓ **No data loss** - Lossless encryption  
✓ **Fast decryption** - In-place processing  
✓ **Component analysis** - Y/Cb/Cr DC coefficient tracking  
✓ **Comprehensive testing** - Full test suite included  
✓ **Clean codebase** - No redundant files  

---

## 📞 Reference

**Project Root:** `/home/minh/Documents/NCKH20262/`

**Bitstream Implementation:** `/home/minh/Documents/NCKH20262/bitstream_impl/`

**Test Results:** `./results/` (1.3 GB)

**Build:** `make` or `./test_and_save_results.sh`

---

**Last Updated:** April 18, 2026  
**Status:** ✅ COMPLETE & TESTED
