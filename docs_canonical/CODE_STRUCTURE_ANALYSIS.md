# Phân Tích Cấu Trúc Code - H.264 Hybrid Encryption

## 📋 Tóm Tắt Nhanh

**File được sử dụng**: 8 files (thực tế không phải 13 như document cũ)
**File KHÔNG sử dụng**: 20+ files (bitio, rbsp, cavlc, arnold_2d, analyze, test, v.v.)
**Trình tự chạy**: Biên dịch → Mã hóa → Giải mã → Xác minh

---

## ✅ CÁC FILE ĐƯỢC SỬ DỤNG (8 files - thực tế)

### 🔧 Các File Core (Bắt buộc - 2 files)

#### 1. **hybrid_encryption.cpp / hybrid_encryption.h**
- **Chức năng**: Thực hiện encryption hybrid (PLCM + Arnold 2D + XOR Diffusion)
-- **Được sử dụng bởi**: S1, S2
- **Các hàm chính**:
  - `generateKeystream()` - Tạo keystream từ password sử dụng PLCM
  - `applyArnold2D()` - Áp dụng Arnold 2D permutation lên DCT coefficients
  - `xorDiffusion()` - XOR keystream với permuted data
- **Yêu cầu**: G++ C++11 trở lên

#### 2. **dc_metadata.cpp / dc_metadata.h**
- **Chức năng**: Quản lý metadata mã hóa
- **Được sử dụng bởi**: Tất cả 3 strategies
- **Các hàm chính**:
  - `writeMetadata()` - Lưu thông tin mã hóa vào .meta file
  - `readMetadata()` - Đọc thông tin mã hóa từ .meta file
- **Output files**:
   - `<input>.s1_hybrid_fixed.meta` (S1)
   - `<input>.s2_hybrid_fixed.meta` (S2)

### 🚀 Pipeline Encryption & Decryption (6 files)

#### 3. **pipeline_hybrid_s1_fixed.cpp** → Binary: `pipeline_hybrid_s1_fixed`
- **Chức năng**: Mã hóa TẤT CẢ frames (Type 1 + Type 5)
- **Input**: `<video.h264>` + `<password>`
- **Output**: `<video.h264>.s1_hybrid_fixed` + `<video.h264>.s1_hybrid_fixed.meta`
- **Frames mã hóa**: ~18,202 frames
- **Thời gian**: ~1.37 giây (cho 228 MB)
- **Bảo mật**: ⭐⭐⭐⭐⭐ (Mạnh nhất)

**Trình tự xử lý trong S1**:
```
1. Read input video file
   ↓
2. Iterate through each NALU
   ↓
3. For each NALU with size ≥ 25 bytes:
   a. Remove emulation prevention bytes
   b. Extract DC coefficients (offset 19-43, 25 bytes)
   c. Generate keystream from password
   d. Apply Arnold 2D permutation
   e. XOR with keystream
   f. Write encrypted DC back
   g. Insert emulation prevention bytes
   ↓
4. Write all NALUs to encrypted file
   ↓
5. Write metadata (.meta file)
```

#### 4. **pipeline_hybrid_s2_fixed.cpp** → Binary: `pipeline_hybrid_s2_fixed`
- **Chức năng**: Mã hóa chỉ I-frames (Type 5)
- **Input**: `<video.h264>` + `<password>`
- **Output**: `<video.h264>.s2_hybrid_fixed` + `<video.h264>.s2_hybrid_fixed.meta`
- **Frames mã hóa**: ~148 I-frames
- **Thời gian**: ~1.18 giây
- **Bảo mật**: ⭐⭐⭐ (Vừa phải)

**Trình tự xử lý trong S2**:
```
1. Read input video file
   ↓
2. Iterate through each NALU
   ↓
3. For each I-frame (Type 5) NALU with size ≥ 25 bytes:
   a. Remove emulation prevention bytes
   b. Extract DC coefficients
   c. Apply PLCM + Arnold 2D + XOR
   d. Insert emulation prevention bytes
   ↓
4. Non-I-frames: copy as-is (no encryption)
   ↓
5. Write all NALUs to encrypted file
   ↓
6. Write metadata
```

#### 5. `pipeline_hybrid_s3_fixed.cpp` (removed)

The S3 pipeline has been removed from the canonical code paths. Historic and experimental S3 variants are archived in `s3_artifacts/` and `old/` if you need to inspect previous implementations.

### 🔓 Pipeline Decryption (S1 / S2)

#### 6. **pipeline_hybrid_decrypt_s1_h264analyze.cpp** → Binary: `pipeline_hybrid_decrypt_s1_h264analyze` (recommended)
- **Chức năng**: Giải mã S1 encrypted file
- **Input**: `<encrypted.s1_hybrid_fixed>` + `<password>`
- **Output**: `<encrypted>.s1_hybrid_fixed.decrypted`
- **Thời gian**: ~1.43 giây

#### 7. **pipeline_hybrid_decrypt_s2_h264analyze.cpp** → Binary: `pipeline_hybrid_decrypt_s2_h264analyze` (recommended)
- **Chức năng**: Giải mã S2 encrypted file
- **Input**: `<encrypted.s2_hybrid_fixed>` + `<password>`
- **Output**: `<encrypted>.s2_hybrid_fixed.decrypted`
- **Thời gian**: ~1.16 giây

<!-- S3 decryptor removed from canonical documentation -->

**Trình tự chung cho tất cả decrypt**:
```
1. Read encrypted file + metadata (.meta)
   ↓
2. Iterate through each NALU
   ↓
3. If NALU was encrypted (theo metadata):
   a. Remove emulation prevention bytes
   b. Extract encrypted DC coefficients
   c. Generate SAME keystream from password
   d. Apply Arnold 2D permutation (inverse)
   e. XOR with keystream (inverse operation)
   f. Write decrypted DC back
   g. Insert emulation prevention bytes
   ↓
4. Unencrypted NALUs: copy as-is
   ↓
5. Write all NALUs to decrypted file
```

---

## ❌ CÁC FILE KHÔNG SỬ DỤNG (20+ files)

Những files này NOT included trong bất kỳ `*_fixed.cpp` file nào:

### Parsing Libraries (5 files - KHÔNG DÙNG):
```
- bitio.cpp / bitio.h         (No #include in pipeline_hybrid_*_fixed)
- rbsp.cpp / rbsp.h           (No #include in pipeline_hybrid_*_fixed)
- cavlc_v2.cpp / cavlc.h      (No #include in pipeline_hybrid_*_fixed)
- arnold_2d.cpp / arnold_2d.h (No #include in pipeline_hybrid_*_fixed)
- encryption.cpp / encryption.h (Replaced by hybrid_encryption)
```

**Lý do loại bỏ**: Không có #include directive trong pipeline_hybrid_*_fixed.cpp files

### Analysis/Test Tools (10+ files - KHÔNG DÙNG):
```
- analyze_video.cpp
- analyze_nalu_detail.cpp
- test_arnold_25.cpp
- test_arnold_only.cpp
- test_hybrid_debug.cpp
- test_hybrid_encryption.cpp
- test_padding_5x5.cpp
- decrypt_arnold2d.cpp
- encrypt_arnold2d.cpp
- check_nalu.cpp
```

**Lý do loại bỏ**: Chỉ dùng để debug/development, không phải production files

### Old Implementation Files (trong tmp_old_files/ - KHÔNG DÙNG):
```
- pipeline_encrypt_strategy1/2/3_with_dc.cpp (Old DC extraction strategy)
- pipeline_hybrid_s1/s2/s3.cpp (Pre-fixed versions)
- pipeline_hybrid_s1_v2_emulation.cpp (Development version)
- pipeline_hybrid_s1_v3_simple.cpp (Development version)
- pipeline_decrypt_preserve_perfect.cpp (Old decrypt logic)
```

**Lý do loại bỏ**: Không có proper emulation prevention handling, gây NALU corruption

### Test Result Files (trong tmp_old_files/ - KHÔNG DÙNG):
```
- CHECK_ENCRYPTION.txt
- DC_COMPARISON_SUMMARY.txt
- DC_Strategy*.h264.* files
- FINAL_*.txt files
- PIPELINE_CLEANUP.txt
- TEST_RESULTS_output.txt
```

**Lý do loại bỏ**: Test result từ các iteration trước, không còn tham khảo giá trị

---

## 🔄 TRÌNH TỰ CHẠY ĐẦY ĐỦ

### Phase 1: Biên Dịch (Compilation)

```bash
cd /home/minh/Documents/NCKH20262/bitstream_impl_arnold2d

# S1
g++ -std=c++11 -O2 -o pipeline_hybrid_s1_fixed \
   pipeline_hybrid_s1_fixed.cpp \
   hybrid_encryption.cpp \
   dc_metadata.cpp

# S1 Decrypt (h264_analyze variant - recommended)
g++ -std=c++11 -O2 -o pipeline_hybrid_decrypt_s1_h264analyze \
   pipeline_hybrid_decrypt_s1_h264analyze.cpp \
   hybrid_encryption.cpp \
   dc_metadata.cpp

# S2
g++ -std=c++11 -O2 -o pipeline_hybrid_s2_fixed \
   pipeline_hybrid_s2_fixed.cpp \
   hybrid_encryption.cpp \
   dc_metadata.cpp

# S2 Decrypt (h264_analyze variant - recommended)
g++ -std=c++11 -O2 -o pipeline_hybrid_decrypt_s2_h264analyze \
   pipeline_hybrid_decrypt_s2_h264analyze.cpp \
   hybrid_encryption.cpp \
   dc_metadata.cpp

<!-- S3 removed: compilation/decrypt steps for S3 are deprecated (see `s3_artifacts/` if needed) -->
```

**Output**: 4 executables (2 encrypt + 2 decrypt)

---

### Phase 2: Mã Hóa (Encryption)

#### Option A: Strategy 1 (Mã hóa TẤT CẢ frames)
```bash
./pipeline_hybrid_s1_fixed output.h264 testkey

# Generates:
# - output.h264.s1_hybrid_fixed (encrypted video)
# - output.h264.s1_hybrid_fixed.meta (metadata)
```

#### Option B: Strategy 2 (Mã hóa chỉ I-frames)
```bash
./pipeline_hybrid_s2_fixed output.h264 testkey

# Generates:
# - output.h264.s2_hybrid_fixed (encrypted video)
# - output.h264.s2_hybrid_fixed.meta (metadata)
```

#### Option C: Strategy 3 (removed)

<!-- Strategy 3 (every 3rd frame) has been removed from the canonical pipelines. -->

**Kết quả mã hóa nội bộ**:

```
S1 Encryption Process (output):
├── Reading NALUs...
├── Frame [1]: Type=1, Size=150 bytes → encrypted (DC found)
├── Frame [2]: Type=1, Size=145 bytes → encrypted
├── ...
├── Frames encrypted: 18,202
├── Frames skipped: 680 (size < 25 bytes)
└── Written to: output.h264.s1_hybrid_fixed

S2 Encryption Process (output):
├── Reading NALUs...
├── Frame [1]: Type=5 (I-frame) → encrypted
├── Frame [2]: Type=1 (P-frame) → NOT encrypted
├── Frame [3]: Type=1 (P-frame) → NOT encrypted
├── ...
├── I-frames encrypted: 148
├── Other frames: 19,201 (passed through)
└── Written to: output.h264.s2_hybrid_fixed

<!-- S3 encryption process removed from canonical docs. -->
```

---

### Phase 3: Giải Mã (Decryption)

```bash
# S1 Decrypt
# Decrypt (h264_analyze variant)
./pipeline_hybrid_decrypt_s1_h264analyze output.h264.s1_hybrid_fixed testkey nalu_info.bin
# Output: output.h264.s1_hybrid_fixed.decrypted

# S2 Decrypt
# Decrypt (h264_analyze variant)
./pipeline_hybrid_decrypt_s2_h264analyze output.h264.s2_hybrid_fixed testkey nalu_info.bin
# Output: output.h264.s2_hybrid_fixed.decrypted

<!-- S3 decryption removed from canonical docs. -->
```

**Lưu ý**: Metadata file (.meta) PHẢI CÓ cùng thư mục với encrypted file để giải mã thành công!

---

### Phase 4: Xác Minh (Verification)

```bash
# Kiểm tra NALU structure của file gốc
/tmp/h264bitstream/.builddir/h264_analyze output.h264 | grep "nal_unit_type:" | sort | uniq -c

# Output:
#   18882 Type 1 (P/B frames)
#     155 Type 5 (I-frames)
#       1 Type 6 (SEI)
#     155 Type 7 (SPS)
#     155 Type 8 (PPS)

# Kiểm tra S1 decrypted file
/tmp/h264bitstream/.builddir/h264_analyze output.h264.s1_hybrid_fixed.decrypted | grep "nal_unit_type:" | sort | uniq -c
# Kỳ vọng: GIỐNG HỆT file gốc ✅

# Kiểm tra S2 decrypted file
/tmp/h264bitstream/.builddir/h264_analyze output.h264.s2_hybrid_fixed.decrypted | grep "nal_unit_type:" | sort | uniq -c
# Kỳ vọng: GIỐNG HỆT file gốc ✅

# Kiểm tra S3 decrypted file
/tmp/h264bitstream/.builddir/h264_analyze output.h264.s3_hybrid_fixed.decrypted | grep "nal_unit_type:" | sort | uniq -c
# Kỳ vọng: GIỐNG HỆT file gốc ✅
```

---

## 📊 Dependency Flow Chart

```
┌─────────────────────────────────────────┐
│         Input Video (H.264)             │
└──────────────┬──────────────────────────┘
               │
        ┌──────┴──────┐
        │             │
        ▼             ▼
   S1_Pipeline   S2_Pipeline   S3_Pipeline
   (Encrypt)     (Encrypt)     (Encrypt)
   
        │              │             │
        ├──────────────┼─────────────┤
        │              │             │
        ▼              ▼             ▼
   DC Extraction (bitio, rbsp, cavlc)
   
        │              │             │
        └──────────────┼─────────────┘
                       │
                       ▼
        ┌──────────────────────────┐
        │  hybrid_encryption.cpp   │
        │  (PLCM + Arnold2D + XOR) │
        └──────────────────────────┘
                       │
        ┌──────────────┴──────────────┐
        │                             │
        ▼                             ▼
   dc_metadata.cpp            Encrypted Output
   (Write .meta file)         (.s1/.s2/.s3 file)
```

---

## 🎯 Execution Timeline

### Toàn bộ quy trình cho 228 MB video:

```
Total Time: ~5 giây (nếu chạy cả 3 strategies)

S1 Process:
├─ Encryption: 1.37s ✓
├─ Decryption: 1.43s ✓
└─ Verification: 0.5s
Total S1: 3.30s

S2 Process:
├─ Encryption: 1.18s ✓
├─ Decryption: 1.16s ✓
└─ Verification: 0.5s
Total S2: 2.84s

S3 Process:
├─ Encryption: 1.18s ✓
├─ Decryption: 1.31s ✓
└─ Verification: 0.5s
Total S3: 3.00s

Overall: ~9 giây (sequential)
          ~3.3 giây (parallel, S1 chạy lâu nhất)
```

---

## ✨ Key Points

### File Sử Dụng vs Không Sử Dụng

| **Category** | **Sử Dụng** | **Không Sử Dụng** | **Lý Do** |
|-------------|-----------|-----------------|---------|
| Encrypt/Decrypt | `pipeline_hybrid_s*_fixed.cpp` (6 files) | `*_old.cpp`, `*_v2.cpp` | Có proper emulation prevention |
| Support | `hybrid_encryption.*`, `dc_metadata.*` (2 files) | `encryption.*`, `bitio.*`, `rbsp.*`, `cavlc.*`, `arnold_2d.*` | 2 files đủ, phần khác không được include |

### Trình Tự Chạy Tóm Tắt

```
1. COMPILE: g++ ... -o pipeline_hybrid_s*_fixed ...
2. ENCRYPT: ./pipeline_hybrid_s*_fixed input.h264 password
3. DECRYPT: ./pipeline_hybrid_decrypt_s*_fixed input.h264.s*_hybrid_fixed password
4. VERIFY: h264_analyze output.h264.s*_hybrid_fixed.decrypted
```

### DC Offset & Size (Quan trọng!)

```
DC_OFFSET: 19 bytes (từ đầu NALU payload)
DC_SIZE:   25 bytes (số byte mã hóa)

Vị trí: [19...43] (25 bytes)

Nếu NALU size < 25 bytes → skip encryption
```

---

## 📝 Ghi Chú

-- ✅ S1 and S2 đều **PRODUCTION READY**
- ✅ File `.meta` BẮTBUỘC để giải mã
- ✅ Password phải **CHÍNH XÁC** giữa encrypt/decrypt
- ❌ Đừng xóa file `.meta` khi giải mã
- ⚠️ File size không đổi (format-preserving encryption)
- ⚠️ NALU structure phải giống hệt original file

