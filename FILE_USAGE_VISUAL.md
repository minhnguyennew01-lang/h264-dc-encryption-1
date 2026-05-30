# 📁 File Usage Analysis - Visual Overview

## 🎯 Main Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     H.264 VIDEO INPUT                           │
│                    (output.h264 - 228 MB)                       │
└──────────────────┬──────────────────────────────────────────────┘
                   │
        ┌──────────┴──────────┬──────────────┐
        │                     │              │
        ▼                     ▼              ▼
   ┌─────────────┐    ┌─────────────┐  ┌─────────────┐
   │   STRATEGY 1│    │  STRATEGY 2 │  │ STRATEGY 3  │
   │ (All Frames)│    │ (I-frames)  │  │ (Every 3rd) │
   └─────────────┘    └─────────────┘  └─────────────┘
        │                     │              │
        └─────────────────────┼──────────────┘
                              │
                    ┌─────────▼─────────┐
                    │ ENCRYPTION ENGINE │
                    │ ─────────────────  │
                    │ hybrid_encryption │
                    │ arnold_2d         │
                    │ dc_metadata       │
                    └─────────┬─────────┘
                              │
        ┌─────────────────────┼──────────────┐
        │                     │              │
        ▼                     ▼              ▼
    ┌─────────┐          ┌─────────┐    ┌─────────┐
    │S1 Output│          │S2 Output│    │S3 Output│
    │.s1_h_f  │          │.s2_h_f  │    │.s3_h_f  │
    │+ .meta  │          │+ .meta  │    │+ .meta  │
    └────┬────┘          └────┬────┘    └────┬────┘
         │                    │             │
    ┌────▼─────────────────────▼─────────────▼────┐
    │      DECRYPTION ENGINE (REVERSE)           │
    │      ─────────────────────────             │
    │      Same algorithm as encryption          │
    │      (uses .meta file for info)            │
    └────┬─────────────────────────────────────┬─┘
         │                                     │
         ▼                                     ▼
    Decrypted Output                    Verification ✅
    (.decrypted)                        h264_analyze
```

---

## 📋 Core Files Explained

### GROUP 1: ENCRYPTION ALGORITHMS (Must have)

```
╔═══════════════════════════════════════════════════════════════════╗
║ hybrid_encryption.cpp/h - THE CORE ENGINE                        ║
╠═══════════════════════════════════════════════════════════════════╣
║                                                                   ║
║ Implements: PLCM Keystream + Arnold 2D + XOR Diffusion          ║
║                                                                   ║
║ Main Functions:                                                   ║
│ ├─ generateKeystream()      Generate PLCM-based keystream        ║
║ ├─ applyArnold2D()         Permute DCT coefficients             ║
║ └─ xorDiffusion()          XOR with keystream                    ║
║                                                                   ║
║ Used by: ALL pipelines (S1, S2, S3, decrypt)                     ║
║                                                                   ║
╚═══════════════════════════════════════════════════════════════════╝

╔═══════════════════════════════════════════════════════════════════╗
║ arnold_2d.cpp/h - PERMUTATION FUNCTION                           ║
╠═══════════════════════════════════════════════════════════════════╣
║                                                                   ║
║ Implements: Arnold Cat Map 5×5 Grid (P=3, Q=11)                 ║
║                                                                   ║
║ Main Functions:                                                   ║
│ ├─ arnold2d_encrypt()      Apply forward permutation             ║
║ └─ arnold2d_decrypt()      Apply inverse permutation             ║
║                                                                   ║
║ Used by: hybrid_encryption.cpp                                    ║
║                                                                   ║
╚═══════════════════════════════════════════════════════════════════╝

╔═══════════════════════════════════════════════════════════════════╗
║ dc_metadata.cpp/h - METADATA MANAGEMENT                          ║
╠═══════════════════════════════════════════════════════════════════╣
║                                                                   ║
║ Stores: Which NALUs were encrypted, offset/size info             ║
║                                                                   ║
║ Output files:                                                     ║
│ ├─ <video>.s1_hybrid_fixed.meta  (S1 metadata)                    ║
║ ├─ <video>.s2_hybrid_fixed.meta  (S2 metadata)                    ║
║ └─ <video>.s3_hybrid_fixed.meta  (S3 metadata)                    ║
║                                                                   ║
║ ⚠️  MUST KEEP with encrypted file for decryption!                ║
║                                                                   ║
╚═══════════════════════════════════════════════════════════════════╝
```

---

### GROUP 2: H.264 PARSING UTILITIES (Required for NALU processing)

```
╔═══════════════════════════════════════════════════════════════════╗
║ bitio.cpp/h - BIT-LEVEL I/O                                      ║
╠═══════════════════════════════════════════════════════════════════╣
║ Purpose: Read/write individual bits from H.264 stream            ║
║ Used by: NALU parsing, entropy decoding                          ║
║                                                                   ║
║ Key Functions:                                                    ║
│ ├─ readBits()    Read N bits                                      ║
║ ├─ writeBits()   Write N bits                                     ║
║ └─ byteAlign()   Align to byte boundary                           ║
║                                                                   ║
╚═══════════════════════════════════════════════════════════════════╝

╔═══════════════════════════════════════════════════════════════════╗
║ rbsp.cpp/h - RAW BYTE SEQUENCE PAYLOAD                            ║
╠═══════════════════════════════════════════════════════════════════╣
║ Purpose: Handle RBSP (emulation prevention bytes)                ║
║ Rule: Insert 0x03 after 0x00 0x00 if next byte ≤ 0x03           ║
║ Used by: NALU payload extraction                                 ║
║                                                                   ║
╚═══════════════════════════════════════════════════════════════════╝

╔═══════════════════════════════════════════════════════════════════╗
║ cavlc_v2.cpp/h - ENTROPY DECODER                                  ║
╠═══════════════════════════════════════════════════════════════════╣
║ Purpose: Decode CAVLC (Context-Adaptive VLC) encoded data        ║
║ Used by: Extracting DCT coefficients from NALU                   ║
║                                                                   ║
╚═══════════════════════════════════════════════════════════════════╝
```

---

### GROUP 3: ENCRYPTION PIPELINES (Choose ONE strategy)

```
┌─────────────────────────────────────────────────────────────────┐
│ STRATEGY 1: pipeline_hybrid_s1_fixed.cpp                         │
├─────────────────────────────────────────────────────────────────┤
│ Feature: Encrypts ALL frames (Type 1 + Type 5)                  │
│                                                                  │
│ Pseudo-code:                                                     │
│ ┌────────────────────────────────────────────────────────────┐  │
│ │ for each NALU in input_video:                              │  │
│ │   if NALU.size >= 25 bytes:                                │  │
│ │     payload = remove_emulation_prevention(NALU)            │  │
│ │     dc_block = payload[19:44]  # 25 bytes                  │  │
│ │                                                             │  │
│ │     keystream = generateKeystream(password)                │  │
│ │     dc_permuted = applyArnold2D(dc_block)                  │  │
│ │     dc_encrypted = dc_permuted XOR keystream               │  │
│ │                                                             │  │
│ │     payload[19:44] = dc_encrypted                          │  │
│ │     payload = insert_emulation_prevention(payload)         │  │
│ │     write_to_output(payload)                               │  │
│ └────────────────────────────────────────────────────────────┘  │
│                                                                  │
│ Output:                                                          │
│   • <video>.s1_hybrid_fixed (encrypted)                         │
│   • <video>.s1_hybrid_fixed.meta (metadata)                     │
│                                                                  │
│ Time: 1.37s (228 MB)    Frames: 18,202     Security: ⭐⭐⭐⭐⭐  │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ STRATEGY 2: pipeline_hybrid_s2_fixed.cpp                         │
├─────────────────────────────────────────────────────────────────┤
│ Feature: Encrypts I-frames ONLY (Type 5)                        │
│                                                                  │
│ Pseudo-code:                                                     │
│ ┌────────────────────────────────────────────────────────────┐  │
│ │ for each NALU in input_video:                              │  │
│ │   if NALU.type == 5 (I-frame) AND NALU.size >= 25:        │  │
│ │     # Encrypt DC as in S1                                  │  │
│ │   else:                                                     │  │
│ │     write_as_is(NALU)  # No encryption                     │  │
│ └────────────────────────────────────────────────────────────┘  │
│                                                                  │
│ Output:                                                          │
│   • <video>.s2_hybrid_fixed (encrypted)                         │
│   • <video>.s2_hybrid_fixed.meta (metadata)                     │
│                                                                  │
│ Time: 1.18s (228 MB)    Frames: 148        Security: ⭐⭐⭐     │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ STRATEGY 3: pipeline_hybrid_s3_fixed.cpp                         │
├─────────────────────────────────────────────────────────────────┤
│ Feature: Encrypts every 3rd frame (Mixed)                       │
│                                                                  │
│ Pseudo-code:                                                     │
│ ┌────────────────────────────────────────────────────────────┐  │
│ │ nalu_counter = 0                                            │  │
│ │ for each NALU in input_video:                              │  │
│ │   if nalu_counter % 3 == 0 AND NALU.size >= 25:           │  │
│ │     # Encrypt DC as in S1                                  │  │
│ │   else:                                                     │  │
│ │     write_as_is(NALU)                                      │  │
│ │   nalu_counter++                                           │  │
│ └────────────────────────────────────────────────────────────┘  │
│                                                                  │
│ Output:                                                          │
│   • <video>.s3_hybrid_fixed (encrypted)                         │
│   • <video>.s3_hybrid_fixed.meta (metadata)                     │
│                                                                  │
│ Time: 1.18s (228 MB)    Frames: 6,055      Security: ⭐⭐⭐⭐   │
└─────────────────────────────────────────────────────────────────┘
```

---

### GROUP 4: DECRYPTION PIPELINES (Match the encryption strategy)

```
┌──────────────────────────────────────────────────────────────────┐
│ DECRYPTION LOGIC (Same for all 3 strategies)                     │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│ pipeline_hybrid_decrypt_s1_fixed.cpp                             │
│ pipeline_hybrid_decrypt_s2_fixed.cpp                             │
│ pipeline_hybrid_decrypt_s3_fixed.cpp                             │
│                                                                   │
│ Common steps:                                                     │
│ ┌────────────────────────────────────────────────────────────┐   │
│ │ load_metadata(input_file.meta)   # Know which NALUs encrypt│   │
│ │                                                             │   │
│ │ for each NALU in encrypted_file:                           │   │
│ │   if metadata[nalu_index].is_encrypted:                    │   │
│ │     payload = remove_emulation_prevention(NALU)            │   │
│ │     dc_encrypted = payload[19:44]                          │   │
│ │                                                             │   │
│ │     keystream = generateKeystream(password)  # SAME!        │   │
│ │     dc_permuted = dc_encrypted XOR keystream  # Reverse XOR│   │
│ │     dc_block = arnold2d_inverse(dc_permuted) # Inverse map │   │
│ │                                                             │   │
│ │     payload[19:44] = dc_block                              │   │
│ │     payload = insert_emulation_prevention(payload)         │   │
│ │     write_to_output(payload)                               │   │
│ │   else:                                                     │   │
│ │     write_as_is(NALU)                                      │   │
│ └────────────────────────────────────────────────────────────┘   │
│                                                                   │
│ Output: <encrypted_file>.decrypted                               │
│                                                                   │
│ ⚠️  MUST have .meta file in same directory!                      │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

---

## 🗑️ Archived Files (Why NOT used)

```
╔═══════════════════════════════════════════════════════════════════╗
║ OLD PIPELINE FILES (tmp_old_files/)                              ║
╠═══════════════════════════════════════════════════════════════════╣
║                                                                   ║
║ ❌ pipeline_hybrid_s1.cpp      → BROKEN (Type 28 corruption)    ║
║ ❌ pipeline_hybrid_s1_v2_emulation.cpp   → Incomplete           ║
║ ❌ pipeline_hybrid_s1_v3_simple.cpp      → Buggy                ║
║ ❌ pipeline_hybrid_s2.cpp      → No proper metadata            ║
║ ❌ pipeline_hybrid_s3.cpp      → Wrong DC offset               ║
║ ❌ pipeline_encrypt_strategy*.cpp   → Old algorithm             ║
║ ❌ pipeline_decrypt_preserve_perfect.cpp → Wrong decryption    ║
║                                                                   ║
║ Reason for removal:                                              ║
│ ├─ No proper emulation prevention handling                       ║
║ ├─ Incorrect DC coefficient offsets                              ║
║ ├─ Missing metadata management                                   ║
║ ├─ Produced Type 28 NALU corruption                              ║
║ └─ Test outputs were inconsistent                                ║
║                                                                   ║
║ ✅ REPLACED by: pipeline_hybrid_s*_fixed.cpp versions           ║
║                                                                   ║
╚═══════════════════════════════════════════════════════════════════╝

╔═══════════════════════════════════════════════════════════════════╗
║ OLD TEST RESULT FILES (tmp_old_files/)                           ║
╠═══════════════════════════════════════════════════════════════════╣
║                                                                   ║
║ ❌ *.txt files (16 files) - Old test outputs from iterations    ║
║ ❌ *.dc_* files - DC extraction results from old pipelines      ║
║                                                                   ║
║ No longer needed - replaced by cleaner verification process      ║
║ with h264_analyze                                                 ║
║                                                                   ║
╚═══════════════════════════════════════════════════════════════════╝
```

---

## 📊 Comparison: Used vs Not Used

```
FEATURE COMPARISON
═════════════════════════════════════════════════════════════════

✅ USED (Fixed)              ❌ NOT USED (Old)
─────────────────            ──────────────────
Emulation prevention         ❌ Missing/broken
DC offset: 19 bytes          DC offset: 20 bytes (wrong)
DC size: 25 bytes            DC size: 24 bytes (wrong)
Metadata handling            ❌ No metadata system
NALU structure preserved     ❌ Type 28 corruption
Format-preserving           ❌ File size mismatch
Performance tested           ❌ Unreliable timing
H.264 compliant             ❌ Non-compliant output
Batch scripts               ❌ Manual testing only
Comprehensive docs          ❌ Minimal documentation
```

---

## 🎯 Decision Tree: Which File to Use?

```
START
  │
  ├─ Do you need to ENCRYPT?
  │  │
  │  └─ YES
  │     │
  │     ├─ Which strategy?
  │     │  │
  │     │  ├─ Maximum security (all frames) → pipeline_hybrid_s1_fixed
  │     │  ├─ Fast streaming (I-frames only) → pipeline_hybrid_s2_fixed
  │     │  └─ Balanced (every 3rd) → pipeline_hybrid_s3_fixed
  │     │
  │     └─ COMPILE: g++ ... <pipeline>.cpp hybrid_encryption.cpp dc_metadata.cpp
  │
  ├─ Do you need to DECRYPT?
  │  │
  │  └─ YES
  │     │
  │     ├─ Which strategy was used?
  │     │  │
  │     │  ├─ S1 → pipeline_hybrid_decrypt_s1_fixed
  │     │  ├─ S2 → pipeline_hybrid_decrypt_s2_fixed
  │     │  └─ S3 → pipeline_hybrid_decrypt_s3_fixed
  │     │
  │     ├─ Check: .meta file exists? ⚠️ REQUIRED!
  │     │
  │     └─ COMPILE: g++ ... <decrypt_pipeline>.cpp hybrid_encryption.cpp dc_metadata.cpp
  │
  └─ END

KEY POINTS:
• Use ONLY *_fixed versions (from main directory)
• NEVER use old files from tmp_old_files/
• ALWAYS keep .meta file with encrypted video
• .meta file must be in same directory as encrypted video
```

---

## 📈 Execution Statistics

```
PERFORMANCE (228 MB video)
═════════════════════════════════════════════════════════════════

S1 (All Frames)
├─ Compile: 2-3 seconds
├─ Encrypt: 1.37 seconds → 18,202 frames encrypted
├─ Decrypt: 1.43 seconds → 18,202 frames decrypted
├─ Verify: 0.5 seconds
└─ Total: ~5.3 seconds

S2 (I-frames only)
├─ Compile: 2-3 seconds
├─ Encrypt: 1.18 seconds → 148 frames encrypted
├─ Decrypt: 1.16 seconds → 148 frames decrypted
├─ Verify: 0.5 seconds
└─ Total: ~4.84 seconds

S3 (Every 3rd)
├─ Compile: 2-3 seconds
├─ Encrypt: 1.18 seconds → 6,055 frames encrypted
├─ Decrypt: 1.31 seconds → 6,055 frames decrypted
├─ Verify: 0.5 seconds
└─ Total: ~4.99 seconds

Overall (All 3 strategies)
├─ Sequential: ~15 seconds
├─ Parallel: ~5.3 seconds (S1 is bottleneck)
└─ Average: ~5 seconds per strategy
```

