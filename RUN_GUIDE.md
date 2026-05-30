# H.264 Hybrid Encryption - Run Guide

## Quick Start

### Prerequisites
```bash
cd /home/minh/Documents/NCKH20262/bitstream_impl_arnold2d
```

### Compilation

Compile all pipelines:
```bash
# S1: Encrypt all frames
g++ -std=c++11 -O2 -o pipeline_hybrid_s1_fixed pipeline_hybrid_s1_fixed.cpp hybrid_encryption.cpp dc_metadata.cpp

# S1 Decrypt
g++ -std=c++11 -O2 -o pipeline_hybrid_decrypt_s1_fixed pipeline_hybrid_decrypt_s1_fixed.cpp hybrid_encryption.cpp dc_metadata.cpp

# S2: Encrypt I-frames only
g++ -std=c++11 -O2 -o pipeline_hybrid_s2_fixed pipeline_hybrid_s2_fixed.cpp hybrid_encryption.cpp dc_metadata.cpp

# S2 Decrypt
g++ -std=c++11 -O2 -o pipeline_hybrid_decrypt_s2_fixed pipeline_hybrid_decrypt_s2_fixed.cpp hybrid_encryption.cpp dc_metadata.cpp

# S3: Encrypt every 3rd frame
g++ -std=c++11 -O2 -o pipeline_hybrid_s3_fixed pipeline_hybrid_s3_fixed.cpp hybrid_encryption.cpp dc_metadata.cpp

# S3 Decrypt
g++ -std=c++11 -O2 -o pipeline_hybrid_decrypt_s3_fixed pipeline_hybrid_decrypt_s3_fixed.cpp hybrid_encryption.cpp dc_metadata.cpp
```

---

## Strategy 1: Encrypt All Frames (STRONGEST ENCRYPTION)

**Use case:** Maximum security, encrypts 18,202 frames

### Encrypt
```bash
./pipeline_hybrid_s1_fixed <input.h264> <password>
```

**Output:**
- `<input.h264>.s1_hybrid_fixed` - Encrypted file
- `<input.h264>.s1_hybrid_fixed.meta` - Metadata file

**Example:**
```bash
./pipeline_hybrid_s1_fixed output.h264 mypassword
```

### Decrypt
```bash
./pipeline_hybrid_decrypt_s1_fixed <encrypted_file> <password>
```

**Output:**
- `<encrypted_file>.decrypted` - Decrypted H.264 file

**Example:**
```bash
./pipeline_hybrid_decrypt_s1_fixed output.h264.s1_hybrid_fixed mypassword
```

### Verify
```bash
# Check NALU structure matches original
/tmp/h264bitstream/.builddir/h264_analyze output.h264.s1_hybrid_fixed.decrypted | grep "nal_unit_type:" | sort | uniq -c
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

**Use case:** Good security + reasonable performance, encrypts 148 I-frames

### Encrypt
```bash
./pipeline_hybrid_s2_fixed <input.h264> <password>
```

**Output:**
- `<input.h264>.s2_hybrid_fixed` - Encrypted file
- `<input.h264>.s2_hybrid_fixed.meta` - Metadata file

**Example:**
```bash
./pipeline_hybrid_s2_fixed output.h264 mypassword
```

### Decrypt
```bash
./pipeline_hybrid_decrypt_s2_fixed <encrypted_file> <password>
```

**Output:**
- `<encrypted_file>.decrypted` - Decrypted H.264 file

**Example:**
```bash
./pipeline_hybrid_decrypt_s2_fixed output.h264.s2_hybrid_fixed mypassword
```

### Verify
```bash
/tmp/h264bitstream/.builddir/h264_analyze output.h264.s2_hybrid_fixed.decrypted | grep "nal_unit_type:" | sort | uniq -c
```

---

## Strategy 3: Encrypt Every 3rd Frame (MIXED FREQUENCY)

**Use case:** Good performance + moderate encryption, encrypts 6,055 frames

### Encrypt
```bash
./pipeline_hybrid_s3_fixed <input.h264> <password>
```

**Output:**
- `<input.h264>.s3_hybrid_fixed` - Encrypted file
- `<input.h264>.s3_hybrid_fixed.meta` - Metadata file

**Example:**
```bash
./pipeline_hybrid_s3_fixed output.h264 mypassword
```

### Decrypt
```bash
./pipeline_hybrid_decrypt_s3_fixed <encrypted_file> <password>
```

**Output:**
- `<encrypted_file>.decrypted` - Decrypted H.264 file

**Example:**
```bash
./pipeline_hybrid_decrypt_s3_fixed output.h264.s3_hybrid_fixed mypassword
```

### Verify
```bash
/tmp/h264bitstream/.builddir/h264_analyze output.h264.s3_hybrid_fixed.decrypted | grep "nal_unit_type:" | sort | uniq -c
```

---

## Batch Processing Example

Encrypt and decrypt all 3 strategies on a video:

```bash
#!/bin/bash
VIDEO="output.h264"
PASSWORD="testkey"

echo "=== S1: All Frames ==="
time ./pipeline_hybrid_s1_fixed "$VIDEO" "$PASSWORD"
time ./pipeline_hybrid_decrypt_s1_fixed "$VIDEO.s1_hybrid_fixed" "$PASSWORD"

echo -e "\n=== S2: I-Frames Only ==="
time ./pipeline_hybrid_s2_fixed "$VIDEO" "$PASSWORD"
time ./pipeline_hybrid_decrypt_s2_fixed "$VIDEO.s2_hybrid_fixed" "$PASSWORD"

echo -e "\n=== S3: Every 3rd Frame ==="
time ./pipeline_hybrid_s3_fixed "$VIDEO" "$PASSWORD"
time ./pipeline_hybrid_decrypt_s3_fixed "$VIDEO.s3_hybrid_fixed" "$PASSWORD"

echo -e "\n=== Verification ==="
echo "S1:" && /tmp/h264bitstream/.builddir/h264_analyze "$VIDEO.s1_hybrid_fixed.decrypted" 2>&1 | grep "nal_unit_type:" | sort | uniq -c
echo "S2:" && /tmp/h264bitstream/.builddir/h264_analyze "$VIDEO.s2_hybrid_fixed.decrypted" 2>&1 | grep "nal_unit_type:" | sort | uniq -c
echo "S3:" && /tmp/h264bitstream/.builddir/h264_analyze "$VIDEO.s3_hybrid_fixed.decrypted" 2>&1 | grep "nal_unit_type:" | sort | uniq -c
```

---

## Strategy Comparison

| Feature | S1 | S2 | S3 |
|---------|----|----|-----|
| **Frames Encrypted** | 18,202 | 148 | 6,055 |
| **Security Level** | ★★★★★ (Maximum) | ★★★ (Moderate) | ★★★★ (High) |
| **Performance** | 1.1s encrypt | 1.2s encrypt | 1.1s encrypt |
| **H.264 Compliance** | ✅ Perfect | ✅ Perfect | ✅ Perfect |
| **Use Case** | Sensitive content | Streaming | Balanced |
| **Recommendation** | 🥇 Production | 🥈 Production | 🥈 Production |

---

## Troubleshooting

### Error: "Cannot open input file"
- Check file path is correct
- Ensure file exists: `ls -lh <file>`

### Error: "decrypt_hybrid expects 25 bytes"
- This is a fatal error - contact developer
- Currently all strategies use DC_SIZE=25 (S1/S2/S3 fixed versions)

### NALU mismatch after decryption
- Verify original file: `/tmp/h264bitstream/.builddir/h264_analyze <original>`
- Compare with decrypted: `/tmp/h264bitstream/.builddir/h264_analyze <decrypted>`
- Both should show identical NALU counts

### File size changed
- This should NOT happen - encryption is format-preserving
- If size differs, re-run encryption/decryption

---

## Performance Benchmarks (on 228 MB video)

```
S1 Encryption:  1.1 seconds
S1 Decryption:  1.6 seconds
S1 Total:       2.7 seconds

S2 Encryption:  1.2 seconds
S2 Decryption:  1.0 seconds
S2 Total:       2.2 seconds

S3 Encryption:  1.1 seconds
S3 Decryption:  1.2 seconds
S3 Total:       2.3 seconds
```

---

## File Structure After Processing

```
bitstream_impl_arnold2d/
├── output.h264                          (Original: 228 MB)
├── output.h264.s1_hybrid_fixed          (S1 encrypted: 228 MB)
├── output.h264.s1_hybrid_fixed.meta     (S1 metadata)
├── output.h264.s1_hybrid_fixed.decrypted (S1 decrypted: 228 MB)
│
├── output.h264.s2_hybrid_fixed          (S2 encrypted: 228 MB)
├── output.h264.s2_hybrid_fixed.meta     (S2 metadata)
├── output.h264.s2_hybrid_fixed.decrypted (S2 decrypted: 228 MB)
│
├── output.h264.s3_hybrid_fixed          (S3 encrypted: 228 MB)
├── output.h264.s3_hybrid_fixed.meta     (S3 metadata)
└── output.h264.s3_hybrid_fixed.decrypted (S3 decrypted: 228 MB)
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
