# 🎯 Quick Reference - File Usage & Execution Order

## 📦 Active Files (13 Core Files)

```
CORE LIBRARIES (6)
├── hybrid_encryption.cpp/.h       [PLCM + Arnold2D + XOR]
├── dc_metadata.cpp/.h              [Metadata management]
├── bitio.cpp/.h                    [Bit stream I/O]
├── rbsp.cpp/.h                     [RBSP parsing]
├── cavlc_v2.cpp/.h                 [Entropy decoding]
└── arnold_2d.cpp/.h                [Arnold 2D permutation]

ENCRYPTION PIPELINES (3)
├── pipeline_hybrid_s1_fixed.cpp    [All frames]
├── pipeline_hybrid_s2_fixed.cpp    [I-frames only]
└── pipeline_hybrid_s3_fixed.cpp    [Every 3rd frame]

DECRYPTION PIPELINES (3)
├── pipeline_hybrid_decrypt_s1_fixed.cpp
├── pipeline_hybrid_decrypt_s2_fixed.cpp
└── pipeline_hybrid_decrypt_s3_fixed.cpp

BUILD
└── Makefile / Makefile.arnold2d
```

---

## 🗑️ Archived Files (25 Old Files)

**Location**: `tmp_old_files/`

```
OLD PIPELINES (9)
├── pipeline_hybrid_s1.cpp
├── pipeline_hybrid_s1_v2_emulation.cpp
├── pipeline_hybrid_s1_v3_simple.cpp
├── pipeline_hybrid_s2.cpp
├── pipeline_hybrid_s3.cpp
├── pipeline_encrypt_strategy1_with_dc.cpp
├── pipeline_encrypt_strategy2_with_dc.cpp
├── pipeline_encrypt_strategy3_with_dc.cpp
└── pipeline_decrypt_preserve_perfect.cpp

TEST RESULTS (16)
├── *.txt files (test outputs from iterations)
└── *.dc_* files (DC extraction results)
```

**Why archived?** Old files lacked proper emulation prevention handling → Type 28 NALU corruption.

---

## ⚡ Execution Sequence

### Step 0: Preparation
```bash
cd /home/minh/Documents/NCKH20262/bitstream_impl_arnold2d
```

### Step 1: Compilation
```bash
# Compile all 6 executables
g++ -std=c++11 -O2 -o pipeline_hybrid_s1_fixed \
    pipeline_hybrid_s1_fixed.cpp hybrid_encryption.cpp dc_metadata.cpp

g++ -std=c++11 -O2 -o pipeline_hybrid_s2_fixed \
    pipeline_hybrid_s2_fixed.cpp hybrid_encryption.cpp dc_metadata.cpp

g++ -std=c++11 -O2 -o pipeline_hybrid_s3_fixed \
    pipeline_hybrid_s3_fixed.cpp hybrid_encryption.cpp dc_metadata.cpp

g++ -std=c++11 -O2 -o pipeline_hybrid_decrypt_s1_fixed \
    pipeline_hybrid_decrypt_s1_fixed.cpp hybrid_encryption.cpp dc_metadata.cpp

g++ -std=c++11 -O2 -o pipeline_hybrid_decrypt_s2_fixed \
    pipeline_hybrid_decrypt_s2_fixed.cpp hybrid_encryption.cpp dc_metadata.cpp

g++ -std=c++11 -O2 -o pipeline_hybrid_decrypt_s3_fixed \
    pipeline_hybrid_decrypt_s3_fixed.cpp hybrid_encryption.cpp dc_metadata.cpp
```

### Step 2: Choose Strategy & Encrypt

**Choose ONE of the following**:

#### 🔒 Strategy 1: Maximum Security (18,202 frames)
```bash
./pipeline_hybrid_s1_fixed output.h264 mypassword
# Output: output.h264.s1_hybrid_fixed (+ .meta)
```

#### 🚀 Strategy 2: Streaming Mode (148 I-frames)
```bash
./pipeline_hybrid_s2_fixed output.h264 mypassword
# Output: output.h264.s2_hybrid_fixed (+ .meta)
```

#### ⚡ Strategy 3: Balanced (6,055 frames every 3rd)
```bash
./pipeline_hybrid_s3_fixed output.h264 mypassword
# Output: output.h264.s3_hybrid_fixed (+ .meta)
```

### Step 3: Decrypt (Use matching strategy)

```bash
# For S1
./pipeline_hybrid_decrypt_s1_fixed output.h264.s1_hybrid_fixed mypassword
# Output: output.h264.s1_hybrid_fixed.decrypted

# For S2
./pipeline_hybrid_decrypt_s2_fixed output.h264.s2_hybrid_fixed mypassword
# Output: output.h264.s2_hybrid_fixed.decrypted

# For S3
./pipeline_hybrid_decrypt_s3_fixed output.h264.s3_hybrid_fixed mypassword
# Output: output.h264.s3_hybrid_fixed.decrypted
```

### Step 4: Verify (Optional)
```bash
/tmp/h264bitstream/.builddir/h264_analyze output.h264.s1_hybrid_fixed.decrypted
```

---

## 📊 Strategy Comparison

| Aspect | S1 | S2 | S3 |
|--------|----|----|-----|
| **Frames Encrypted** | 18,202 | 148 | 6,055 |
| **Frame Types** | All | I-frames only | Every 3rd |
| **Security** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ |
| **Speed** | 1.37s enc | 1.18s enc | 1.18s enc |
| **Use Case** | Sensitive content | Real-time streaming | Balanced |
| **File Size Change** | None | None | None |

---

## 🔍 File Dependencies Map

```
Input Video (H.264)
    ↓
    ├─→ [Choose Strategy: S1, S2, or S3]
    ↓
┌───────────────────────────────────────┐
│ Pipeline Encrypt (1.1-1.4s)           │
├───────────────────────────────────────┤
│ 1. Read NALU (bitio, rbsp, cavlc)     │
│ 2. Extract DC coefficients            │
│ 3. PLCM keystream (hybrid_encryption) │
│ 4. Arnold2D permutation (arnold_2d)   │
│ 5. XOR diffusion (hybrid_encryption)  │
│ 6. Write encrypted NALU               │
│ 7. Save metadata (dc_metadata)        │
└───────────────────────────────────────┘
    ↓
Encrypted File (.s1/.s2/.s3_hybrid_fixed)
+ Metadata File (.meta) [REQUIRED]
    ↓
    └─→ [Matched Decrypt: S1, S2, or S3]
    ↓
┌───────────────────────────────────────┐
│ Pipeline Decrypt (1.1-1.4s)           │
├───────────────────────────────────────┤
│ 1. Read encrypted NALU                │
│ 2. Read metadata (dc_metadata)        │
│ 3. Generate same keystream            │
│ 4. Inverse Arnold2D (arnold_2d)       │
│ 5. XOR diffusion (reverse)            │
│ 6. Write decrypted NALU               │
└───────────────────────────────────────┘
    ↓
Decrypted File (.decrypted)
    ↓
    └─→ Verify NALU Structure ✅
```

---

## ✅ Verification Checklist

Before deployment:
- [ ] All 6 binaries compiled successfully
- [ ] Encrypted file created (output.h264.s*_hybrid_fixed)
- [ ] Metadata file created (output.h264.s*_hybrid_fixed.meta)
- [ ] Decrypted file created (output.h264.s*_hybrid_fixed.decrypted)
- [ ] h264_analyze shows identical NALU structure
- [ ] File size unchanged (format-preserving)

---

## 🚨 Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| "Metadata file not found" | Deleted `.meta` file | Don't delete! Keep with encrypted file |
| "Wrong password" | Password mismatch | Use exact same password for decrypt |
| "Type 28 NALU corruption" | Using old pipeline files | Use `*_fixed` versions only |
| "Segmentation fault" | NALU size < 25 bytes | Pipeline skips automatically |
| "File size mismatch" | Emulation prevention issue | Should be identical size |

---

## 💾 File Locations Summary

```
/home/minh/Documents/NCKH20262/bitstream_impl_arnold2d/

ACTIVE SOURCE FILES (*.cpp *.h)
├── hybrid_encryption.* (CORE)
├── dc_metadata.* (CORE)
├── bitio.* (SUPPORT)
├── rbsp.* (SUPPORT)
├── cavlc_v2.* (SUPPORT)
├── arnold_2d.* (CORE)
├── pipeline_hybrid_s1_fixed.cpp
├── pipeline_hybrid_s2_fixed.cpp
├── pipeline_hybrid_s3_fixed.cpp
├── pipeline_hybrid_decrypt_s1_fixed.cpp
├── pipeline_hybrid_decrypt_s2_fixed.cpp
└── pipeline_hybrid_decrypt_s3_fixed.cpp

EXECUTABLES (compiled binaries)
├── pipeline_hybrid_s1_fixed
├── pipeline_hybrid_s2_fixed
├── pipeline_hybrid_s3_fixed
├── pipeline_hybrid_decrypt_s1_fixed
├── pipeline_hybrid_decrypt_s2_fixed
└── pipeline_hybrid_decrypt_s3_fixed

ARCHIVED OLD FILES
└── tmp_old_files/ (25 files)

OUTPUT FILES
├── output.h264 (input video)
├── output.h264.s1_hybrid_fixed (encrypted)
├── output.h264.s1_hybrid_fixed.meta (metadata - IMPORTANT!)
├── output.h264.s1_hybrid_fixed.decrypted (decrypted result)
└── [Similar for S2, S3]
```

---

## 🎬 Full Workflow Example

```bash
# 1. Navigate
cd /home/minh/Documents/NCKH20262/bitstream_impl_arnold2d

# 2. Compile all
g++ -std=c++11 -O2 -o pipeline_hybrid_s1_fixed \
    pipeline_hybrid_s1_fixed.cpp hybrid_encryption.cpp dc_metadata.cpp
# ... (repeat for s2, s3, decrypt versions)

# 3. Encrypt with S1
time ./pipeline_hybrid_s1_fixed output.h264 testkey
# Real: 0m1.370s

# 4. Decrypt S1
time ./pipeline_hybrid_decrypt_s1_fixed output.h264.s1_hybrid_fixed testkey
# Real: 0m1.430s

# 5. Verify
/tmp/h264bitstream/.builddir/h264_analyze output.h264.s1_hybrid_fixed.decrypted | grep "nal_unit_type:" | sort | uniq -c
# Should match original exactly ✅

Total Time: ~3.3 seconds for 228 MB video
```

