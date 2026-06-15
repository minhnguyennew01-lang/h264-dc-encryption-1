# H.264 Hybrid Encryption - Run Guide (h264_analyze Version)

## Quick Start

> **⚠️ CRITICAL**: This guide uses h264_analyze-based pipelines (versions ending with `_h264analyze`).  
> **Setup Required**:
 1. h264_analyze must be compiled and installed persistently at `third_party/h264bitstream/.libs/h264_analyze` (recommended). The wrapper will auto-build into `third_party/h264bitstream` if missing.
> 2. Run `./extract_nalu_from_h264analyze <input.h264>` BEFORE any encryption
 2. Run `./extract_nalu_from_h264analyze <input.h264>` BEFORE any encryption (use the wrapper recommended below)
> 3. This generates `nalu_info.bin` (19,348 NALUs for output.h264)

### Prerequisites & Setup

```bash
cd /home/minh/Documents/NCKH20262/bitstream_impl_arnold2d

# Step 0A: Install h264_analyze (one-time setup — recommended persistent install)
# Option: Use the provided setup script to build into `third_party/h264bitstream` (no sudo required):
cd /home/minh/Documents/NCKH20262/bitstream_impl_arnold2d
./scripts/setup_h264_analyze.sh
export LD_LIBRARY_PATH=$(pwd)/third_party/h264bitstream/.libs:$LD_LIBRARY_PATH
# Binary ready: `third_party/h264bitstream/.libs/h264_analyze`

# Step 0B: Return to project and generate NALU list
cd /home/minh/Documents/NCKH20262/bitstream_impl_arnold2d
# Option 1: Use the smart wrapper (recommended) — will auto-build h264_analyze into
# `third_party/h264bitstream` if missing and then run extraction.
./extract_nalu_from_h264analyze_wrapper.sh output.h264

# Option 2: Call the extractor directly (if you have h264_analyze available)
# H264_ANALYZE_BIN environment can point to the binary path. Example:
# H264_ANALYZE_BIN=$(pwd)/third_party/h264bitstream/.libs/h264_analyze ./extract_nalu_from_h264analyze output.h264
# Expected output: ✅ Extracted 19348 NALUs from h264_analyze
# Generated file: nalu_info.bin
```

### Compilation (h264analyze Versions)

Compile h264analyze-based pipelines (recommended):
```bash
# S1: Encrypt P/B frames + I-frames (using h264_analyze)
g++ -std=c++11 -O2 -o pipeline_hybrid_s1_h264analyze \
    pipeline_hybrid_s1_h264analyze.cpp \
    hybrid_encryption.cpp dc_metadata.cpp

# S2: Encrypt I-frames only (using h264_analyze)
g++ -std=c++11 -O2 -o pipeline_hybrid_s2_h264analyze \
    pipeline_hybrid_s2_h264analyze.cpp \
    hybrid_encryption.cpp dc_metadata.cpp

# (S3 removed from project; only S1 and S2 are supported)

## Decryptors (h264_analyze variants)
We provide `_h264analyze` decryptors that use the authoritative `nalu_info.bin` produced by
`extract_nalu_from_h264analyze`. Compile and use these decryptors as shown below.

```bash
# Compile decryptors (one-time)
g++ -std=c++11 -O2 -o pipeline_hybrid_decrypt_s1_h264analyze \
  pipeline_hybrid_decrypt_s1_h264analyze.cpp \
  hybrid_encryption.cpp

g++ -std=c++11 -O2 -o pipeline_hybrid_decrypt_s2_h264analyze \
  pipeline_hybrid_decrypt_s2_h264analyze.cpp \
  hybrid_encryption.cpp

<!-- S3 decryptor deprecated and removed -->
```

Usage (pass `nalu_info.bin` path if needed):

```bash
# S1
./pipeline_hybrid_decrypt_s1_h264analyze output.h264.s1_hybrid_fixed testkey nalu_info.bin

# S2
./pipeline_hybrid_decrypt_s2_h264analyze output.h264.s2_hybrid_fixed testkey nalu_info.bin

# S3
<!-- S3 has been removed from the project. -->
```
```

**Legacy (archived):**

Legacy heuristic-based decryptors (the older `*_fixed` binaries) have been archived into
`archived_unused_pipelines/`. They do not use `nalu_info.bin` and are less reliable. Use the
`_h264analyze` variants above (they are authoritative and require `nalu_info.bin`).

```

---

## Strategy 1: Encrypt All Frames (STRONGEST ENCRYPTION)

**Coverage**: 19,037 NALUs encrypted (Type 1 P/B + Type 5 I-frames)  
**Use case**: Maximum security, encrypts entire video

### Encrypt (h264analyze version)
```bash
# Prerequisite: nalu_info.bin must exist
./pipeline_hybrid_s1_h264analyze output.h264 testkey
```

**Output:**
- `output.h264.s1_hybrid_fixed` - Encrypted file (228 MB)
- `output.h264.s1_hybrid_fixed.meta` - Metadata file

**Expected result:**
```
📊 Found 19348 NAL units (from h264_analyze)
🔐 Processing NALUs...
✅ Fixed encryption complete!
  📊 Total NALUs: 19348
  🔐 Encrypted: 19037
```

### Decrypt (h264analyze version)
```bash
./pipeline_hybrid_decrypt_s1_h264analyze output.h264.s1_hybrid_fixed testkey nalu_info.bin
```

**Output:**
- `output.h264.s1_hybrid_fixed.decrypted` - Decrypted H.264 file

### Verify
```bash
export LD_LIBRARY_PATH=$(pwd)/third_party/h264bitstream/.libs:$LD_LIBRARY_PATH
$(pwd)/third_party/h264bitstream/.libs/h264_analyze output.h264.s1_hybrid_fixed.decrypted 2>&1 | grep "nal_unit_type:" | sort | uniq -c
```

**Expected output:**
```
  18882 Type 1 (P/B frames)
    155 Type 5 (I-frames)
      1 Type 6 (SEI)
    155 Type 7 (SPS)
    155 Type 8 (PPS)
```

---

## Strategy 2: Encrypt I-Frames Only (BALANCED)

**Coverage**: 155 I-frames encrypted (Type 5 only)  
**Use case**: Good security + minimal performance impact

### Encrypt (h264analyze version)
```bash
./pipeline_hybrid_s2_h264analyze output.h264 testkey
```

**Output:**
- `output.h264.s2_hybrid_fixed` - Encrypted file (228 MB)
- `output.h264.s2_hybrid_fixed.meta` - Metadata file

**Expected result:**
```
📊 Found 19348 NAL units (from h264_analyze)
✅ Fixed encryption complete!
  📊 Total NALUs: 19348
  🔐 Encrypted (Type 5 only): 155
```

### Decrypt (h264analyze version)
```bash
./pipeline_hybrid_decrypt_s2_h264analyze output.h264.s2_hybrid_fixed testkey nalu_info.bin
```

### Verify
```bash
export LD_LIBRARY_PATH=$(pwd)/third_party/h264bitstream/.libs:$LD_LIBRARY_PATH
$(pwd)/third_party/h264bitstream/.libs/h264_analyze output.h264.s2_hybrid_fixed.decrypted 2>&1 | grep "nal_unit_type:" | sort | uniq -c
```

---

## Strategy 3: Encrypt Every 3rd Frame (MIXED FREQUENCY)

**Coverage**: 6,342 frames encrypted (selective frame-based approach)  
**Use case**: Balanced security + good performance

### Note: S3 removed

The S3 strategy (every 3rd frame) has been removed from the project and is no longer supported. Use S1 (all frames) or S2 (I-frames only) instead.

### Byte-exactness checks (sha256 / cmp)
After decrypting with the `_h264analyze` tools you can verify byte-exact recovery vs the original `output.h264`:

```bash
sha256sum output.h264 output.h264.s1_hybrid_fixed.decrypted output.h264.s2_hybrid_fixed.decrypted

# Quick byte-compare (silent): exit code 0 => identical
if cmp -s output.h264 output.h264.s1_hybrid_fixed.decrypted; then echo "S1: identical"; else echo "S1: DIFFER"; fi
if cmp -s output.h264 output.h264.s2_hybrid_fixed.decrypted; then echo "S2: identical"; else echo "S2: DIFFER"; fi

# Note: S3 has been removed from the project; no S3 verification steps are provided here.
```

Current verification results (run on June 15, 2026):

```
sha256sum output.h264                       = 263cba305b1fad3590fed532e6cb98f4e3fd7ac7ee0e5a301a7a300eebd9f899
sha256sum output.h264.s1_hybrid_fixed.decrypted = 263cba305b1fad3590fed532e6cb98f4e3fd7ac7ee0e5a301a7a300eebd9f899  (IDENTICAL)
sha256sum output.h264.s2_hybrid_fixed.decrypted = 263cba305b1fad3590fed532e6cb98f4e3fd7ac7ee0e5a301a7a300eebd9f899  (IDENTICAL)
 
Result summary:
- S1: byte-exact match ✅
- S2: byte-exact match ✅
```

---

## Batch Processing Example (h264analyze versions)

Encrypt all 3 strategies on a video using h264analyze-based pipelines:

```bash
#!/bin/bash
VIDEO="output.h264"
PASSWORD="testkey"

# Setup h264_analyze path (persistent install)
export LD_LIBRARY_PATH=$(pwd)/third_party/h264bitstream/.libs:$LD_LIBRARY_PATH

# Step 0: Generate NALU list (one-time, required)
./extract_nalu_from_h264analyze "$VIDEO"

echo "=== S1: All Frames (h264analyze) ==="
time ./pipeline_hybrid_s1_h264analyze "$VIDEO" "$PASSWORD"
# Decrypt (h264_analyze variant)
time ./pipeline_hybrid_decrypt_s1_h264analyze "$VIDEO.s1_hybrid_fixed" "$PASSWORD" nalu_info.bin

echo -e "\n=== S2: I-Frames Only (h264analyze) ==="
time ./pipeline_hybrid_s2_h264analyze "$VIDEO" "$PASSWORD"
time ./pipeline_hybrid_decrypt_s2_h264analyze "$VIDEO.s2_hybrid_fixed" "$PASSWORD" nalu_info.bin

echo -e "\n=== Verification ==="
echo "S1:" && $(pwd)/third_party/h264bitstream/.libs/h264_analyze "$VIDEO.s1_hybrid_fixed.decrypted" 2>&1 | grep "nal_unit_type:" | sort | uniq -c
echo "S2:" && $(pwd)/third_party/h264bitstream/.libs/h264_analyze "$VIDEO.s2_hybrid_fixed.decrypted" 2>&1 | grep "nal_unit_type:" | sort | uniq -c
```

---

## Strategy Comparison (h264analyze - Actual Results)

| Feature | S1 | S2 |
|---------|----|----|
| **NALUs Encrypted** | 19,037 | 155 |
| **Type Coverage** | P/B + I | I-only |
| **Security Level** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ |
| **Encrypt Time** | ~2.5s | ~1.5s |
| **Performance Impact** | High | Minimal |
| **H.264 Compliance** | ✅ Perfect | ✅ Perfect | ✅ Perfect |
| **Best For** | Maximum security | Streaming | Balanced approach |

---

## ⚠️ Known Issues & Limitations

### Decrypt Tools (h264analyze versions)
- **Status**: Available — use the `_h264analyze` decryptors (S1/S2/S3) which read `nalu_info.bin`.
- **Note**: Legacy `_fixed` tools remain in `archived_unused_pipelines/` for reference but are no longer recommended.

### Metadata Format
- **Encrypt**: Generates simple text metadata (Strategy: S1/S2/S3, NALU counts)
- **Decrypt**: Expects CSV format with padding info
# Kiểm tra binary persistent trong repo
test -x third_party/h264bitstream/.libs/h264_analyze && echo "persistent h264_analyze present" || echo "no persistent h264_analyze"

# Kiểm tra binary hiện đang dùng (wrapper có thể đặt ở /tmp)
which h264_analyze || echo "system h264_analyze not on PATH"- **Fix needed**: Align metadata formats across encrypt/decrypt tools

### h264_analyze Setup
- **Recommended persistent path**: `third_party/h264bitstream/.libs/h264_analyze` (use `./scripts/setup_h264_analyze.sh`)
- **LD_LIBRARY_PATH**: Must include `third_party/h264bitstream/.libs` (the wrapper and setup script set this for you)
- **One-time setup**: Run `./scripts/setup_h264_analyze.sh` or use the wrapper which will auto-build if missing

---

## Troubleshooting

### Error: "Found 0 NAL units"
- **Cause**: `extract_nalu_from_h264analyze` cannot find h264_analyze
- **Fix**: 
  ```bash
  # Recommended: build persistent analyzer and use wrapper
  ./scripts/setup_h264_analyze.sh
  ./extract_nalu_from_h264analyze_wrapper.sh output.h264

  # Or (manual): ensure h264_analyze is built and LD_LIBRARY_PATH points at it
  export LD_LIBRARY_PATH=$(pwd)/third_party/h264bitstream/.libs:$LD_LIBRARY_PATH
  $(pwd)/third_party/h264bitstream/.libs/h264_analyze output.h264
  ```

### Error: "Metadata not for S1 h264analyze version"
- **Cause**: Decrypt tool expecting CSV metadata but encrypt generated text format
- **Status**: Known incompatibility (decrypt tools in development)
- **Workaround**: Use legacy `_fixed` decrypt tools

### Error: "Cannot open input file"
- **Cause**: File path incorrect or file doesn't exist
- **Fix**: 
  ```bash
  ls -lh output.h264
  # Must show file size ~228 MB
  ```

### File size mismatch after decryption
- **Expected**: Decrypted file size = Original file size (format-preserving encryption)
- **If different**: Decryption failed - check metadata and key

### NALU count mismatch
- **Verify original**: 
  ```bash
  export LD_LIBRARY_PATH=$(pwd)/third_party/h264bitstream/.libs:$LD_LIBRARY_PATH
  $(pwd)/third_party/h264bitstream/.libs/h264_analyze output.h264 2>&1 | grep -c "Found NAL"
  ```
- **Verify decrypted**: Same command on `.decrypted` file
- **Expected**: Both should match (19,348 NALUs total)

---

## Performance Benchmarks (on 228 MB video with h264analyze)

```
S1 Encryption (19,037 NALUs):   ~2.5 seconds
S2 Encryption (155 I-frames):   ~1.5 seconds


Decryption (using legacy _fixed): Similar timing
Verification with h264_analyze:  ~3-5 seconds per file
```

---

## File Structure After Processing (h264analyze)

```
bitstream_impl_arnold2d/
├── output.h264                          (Original: 228 MB)
├── nalu_info.bin                        (NALU list from h264_analyze: ~19,348 entries)
│
├── output.h264.s1_hybrid_fixed          (S1 encrypted: 228 MB)
├── output.h264.s1_hybrid_fixed.meta     (S1 metadata: text format)
├── output.h264.s1_hybrid_fixed.decrypted (S1 decrypted: 228 MB)
│
├── output.h264.s2_hybrid_fixed          (S2 encrypted: 228 MB)
├── output.h264.s2_hybrid_fixed.meta     (S2 metadata: text format)
├── output.h264.s2_hybrid_fixed.decrypted (S2 decrypted: 228 MB)
│
<!-- S3 artifacts removed from canonical file listing -->
```

---

## Algorithm Details

### Encryption Process (h264analyze version)
1. Run `extract_nalu_from_h264analyze` to parse file with h264_analyze → `nalu_info.bin`
2. Load NALU locations and types from binary file
3. For each NALU (by strategy):
   - S1: Encrypt Type 1 (P/B) + Type 5 (I) = 19,037 NALUs
   - S2: Encrypt Type 5 (I-frames only) = 155 NALUs  
  - S3: removed from project
4. Extract DC coefficients (bytes 19-43, 25 bytes per NALU)
5. Apply hybrid encryption:
   - PLCM Chaotic Keystream (μ=0.37, 1000 iterations)
   - Arnold 2D Cat Map (5×5, P=3, Q=11)
   - XOR Diffusion
6. Replace encrypted DC back into NALU payload
7. Write encrypted file (format-preserving, same size)
8. Save metadata with strategy info and NALU counts

### Key Properties
- **Authoritative NALU detection**: Uses h264_analyze (verified 19,348 NALUs)
- **Format-preserving**: Output file size = input file size
- **NALU-compliant**: All NALUs maintain H.264 structure
- **Selective encryption**: Only DC blocks encrypted
- **Password-based**: Uses SHA256 key derivation from password

---

## Documentation References

- **[COMPLETE_GUIDE.md](COMPLETE_GUIDE.md)** - Detailed technical guide
- **[H264ANALYZE_INTEGRATION_SOLUTION.md](H264ANALYZE_INTEGRATION_SOLUTION.md)** - h264_analyze integration notes
- **[NALU_DETECTION_ANALYSIS.md](NALU_DETECTION_ANALYSIS.md)** - NALU detection methodology
- **[ENCRYPTED_FILE_ANALYSIS.md](ENCRYPTED_FILE_ANALYSIS.md)** - Encryption verification results

---

## Notes

-- **S1 and S2 (h264analyze) are tested and supported** ✅
-- **Encryption pipelines working**: S1 (19,037), S2 (155) NALUs encrypted
- **Decryption pipelines**: In development (metadata format mismatch needs resolution)
- **Metadata file (.meta) is required** for decryption
- **Do NOT modify encrypted files** - corruption will prevent decryption
- **h264_analyze setup is required** - one-time setup at `/tmp/h264bitstream`

---

**Last Updated**: June 15, 2026  
**Version**: 4.0 (h264analyze integration, working encryption pipelines)  
**Status**: Encryption ✅ | Decryption 🔄 (in progress)
<!-- trailing S3 artifact listing removed -->
```

---

## Algorithm Details

### Encryption Process
1. Parse H.264 file to extract NALUs
2. Identify start codes (0x000001 or 0x00000001)
3. Extract NALU payload and remove emulation prevention bytes
4. Encrypt DC (luma DC coefficients) at offset 19-43 bytes using:
   - PLCM Chaotic Keystream (μ=0.37, 1000 iterations)
   - Arnold 2D Cat Map (5×5, P=3, Q=11)
   - XOR Diffusion
5. Re-insert emulation prevention bytes
6. Verify size match before replacing in file
7. Save metadata for decryption

### Key Properties
- **Format-preserving**: Output file size = input file size
- **NALU-compliant**: All NALUs maintain H.264 structure
- **Selective encryption**: Only DC blocks encrypted
- **Password-based**: Uses SHA256 key derivation

---

## Notes

- **All 3 strategies are production-ready** ✅
- S1 recommended for maximum security
- S2 recommended for streaming (low overhead)
- S3 recommended for balanced approach
- Metadata file (.meta) is required for decryption
- Do NOT modify encrypted files - corruption will prevent decryption

---

Last Updated: May 26, 2026
Version: 3.0 (Fixed - Final)
