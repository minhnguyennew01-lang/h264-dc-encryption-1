# 🎯 Hướng Dẫn Chạy Mã Hóa & Giải Mã (S1, S2) - Chi Tiết Đầy Đủ

> **⚠️ Prerequisite**: Before running `pipeline_hybrid_*_h264analyze` pipelines, you MUST run `./extract_nalu_from_h264analyze <input.h264>` to generate `nalu_info.bin`. This file is required for accurate NALU detection using h264_analyze.  
> **Note**: This project uses h264_analyze as the authoritative NALU parser (19,348 NALUs verified).

## 📌 Tóm Tắt Nhanh

| Strategy | Encrypt Time | Decrypt Time | Frames Mã Hóa | Bảo Mật | Tốc Độ |
|----------|--------------|--------------|---------------|---------|--------|
| **S1** | 1.37s | 1.43s | ~18,202 (Tất cả P/B+I) | ⭐⭐⭐⭐⭐ | Chậm |
| **S2** | 1.18s | 1.16s | ~148 (Chỉ I-frames) | ⭐⭐⭐ | Nhanh |

--

## 🚀 Bước 1: Biên Dịch (Compile)

### 1.1 Biên dịch S1 (Encrypt + Decrypt)

```bash
cd /home/minh/Documents/NCKH20262/bitstream_impl_arnold2d

# Encrypt S1
g++ -std=c++11 -O2 -o pipeline_hybrid_s1_fixed \
    pipeline_hybrid_s1_fixed.cpp \
    hybrid_encryption.cpp \
    dc_metadata.cpp

# Decrypt S1
g++ -std=c++11 -O2 -o pipeline_hybrid_decrypt_s1_h264analyze \
  pipeline_hybrid_decrypt_s1_h264analyze.cpp \
    hybrid_encryption.cpp \
    dc_metadata.cpp
```

### 1.2 Biên dịch S2 (Encrypt + Decrypt)

```bash
# Encrypt S2
g++ -std=c++11 -O2 -o pipeline_hybrid_s2_fixed \
    pipeline_hybrid_s2_fixed.cpp \
    hybrid_encryption.cpp \
    dc_metadata.cpp

# Decrypt S2
g++ -std=c++11 -O2 -o pipeline_hybrid_decrypt_s2_h264analyze \
  pipeline_hybrid_decrypt_s2_h264analyze.cpp \
    hybrid_encryption.cpp \
    dc_metadata.cpp
```

### 1.3 Biên dịch S3 (Encrypt + Decrypt)

```bash
# S3 has been removed from the project; S3 compile/decrypt steps are deprecated.
```

### 1.4 Hoặc Biên dịch Tất Cả Cùng Lúc

```bash
make -f Makefile.arnold2d clean
make -f Makefile.arnold2d all
```

**Kết quả**: 4 executables
- `pipeline_hybrid_s1_fixed` (S1 encrypt)
- `pipeline_hybrid_decrypt_s1_h264analyze` (S1 decrypt - recommended)
- `pipeline_hybrid_s2_fixed` (S2 encrypt)
- `pipeline_hybrid_decrypt_s2_h264analyze` (S2 decrypt - recommended)

---

## 🔐 Bước 2: Mã Hóa (Encryption)

### 2.1 Strategy 1 - Mã Hóa TẤT CẢ Frames

**Chức năng**: Mã hóa tất cả Type 1 (P/B frames) + Type 5 (I-frames) NALUs

```bash
./pipeline_hybrid_s1_fixed output.h264 testkey
```

**Output**:
```
Reading input video file: output.h264
Processing NALUs...
Frame [1]: Type=1, Size=150 bytes
  → Encrypting DC coefficients (offset 19-43, 25 bytes)
  → NALU Type: 1 (P-frame)
...
Frames encrypted: 18,202
Frames skipped (size < 25 bytes): 680
Written to: output.h264.s1_hybrid_fixed
Metadata written to: output.h264.s1_hybrid_fixed.meta
```

**Files Generated**:
- `output.h264.s1_hybrid_fixed` (219 MB - encrypted video)
- `output.h264.s1_hybrid_fixed.meta` (72 KB - metadata)

**Kích thước**: 219M (giống file gốc)
**Thời gian**: ~1.37 giây
**Bảo mật**: Rất cao (mã hóa 18,202 NALUs)

---

### 2.2 Strategy 2 - Mã Hóa Chỉ I-Frames

**Chức năng**: Mã hóa chỉ Type 5 (I-frames) NALUs, pass-through P/B frames

```bash
./pipeline_hybrid_s2_fixed output.h264 testkey
```

**Output**:
```
Reading input video file: output.h264
Processing NALUs...
Frame [1]: Type=5, Size=8500 bytes
  → Encrypting DC coefficients
  → NALU Type: 5 (I-frame) ✓
Frame [2]: Type=1, Size=150 bytes
  → NOT encrypted (P-frame)
  → NALU Type: 1 (P-frame) ✗
...
I-frames encrypted: 148
P/B-frames passed through: 19,201
Written to: output.h264.s2_hybrid_fixed
Metadata written to: output.h264.s2_hybrid_fixed.meta
```

**Files Generated**:
- `output.h264.s2_hybrid_fixed` (219 MB - encrypted video)
- `output.h264.s2_hybrid_fixed.meta` (596 bytes - tiny metadata)

**Kích thước**: 219M
**Thời gian**: ~1.18 giây
**Bảo mật**: Vừa phải (chỉ 148 I-frames)

---

### 2.3 Strategy 3 - Mã Hóa Tất Cả I-Frames + P/B-Frames

**Chức năng**: Mã hóa TẤT CẢ Type 1 (P/B frames) + Type 5 (I-frames) - giống S1 nhưng được thiết kế khác

<!-- Strategy 3 removed: this section has been deprecated and removed from canonical instructions. -->
```

## 🔓 Bước 3: Giải Mã (Decryption)

### ⚠️ QUAN TRỌNG: Metadata File

Giải mã **PHẢI CÓ** file `.meta` cùng thư mục với file mã hóa!

```
output.h264.s1_hybrid_fixed       ← File mã hóa
output.h264.s1_hybrid_fixed.meta  ← File metadata (BẮT BUỘC)
```

### 3.1 Giải Mã S1

```bash
./pipeline_hybrid_decrypt_s1_h264analyze output.h264.s1_hybrid_fixed testkey nalu_info.bin
```

**Output**:
```
Reading encrypted file: output.h264.s1_hybrid_fixed
Reading metadata from: output.h264.s1_hybrid_fixed.meta
  Metadata entries: 18,202
Processing NALUs...
NALU[1]: Type=1 (encrypted)
  → Decrypting DC coefficients
  → NALU Type: 1 (P-frame)
NALU[2]: Type=5 (encrypted)
  → Decrypting DC coefficients
  → NALU Type: 5 (I-frame)
...
NALUs decrypted: 18,202
NALUs passed through: 1,147
Written to: output.h264.s1_hybrid_fixed.decrypted
```

**Files Generated**:
- `output.h264.s1_hybrid_fixed.decrypted` (219 MB - original video)

**Thời gian**: ~1.43 giây

---

### 3.2 Giải Mã S2

```bash
./pipeline_hybrid_decrypt_s2_h264analyze output.h264.s2_hybrid_fixed testkey nalu_info.bin
```

**Output**:
```
Reading encrypted file: output.h264.s2_hybrid_fixed
Reading metadata from: output.h264.s2_hybrid_fixed.meta
  Metadata entries: 148
Processing NALUs...
NALU[1]: Type=5 (encrypted)
  → Decrypting DC coefficients
  → NALU Type: 5 (I-frame)
NALU[2]: Type=1 (not encrypted)
  → Copying as-is
  → NALU Type: 1 (P-frame)
...
NALUs decrypted: 148 (I-frames only)
NALUs passed through: 19,201
Written to: output.h264.s2_hybrid_fixed.decrypted
```

**Files Generated**:
- `output.h264.s2_hybrid_fixed.decrypted` (219 MB)

**Thời gian**: ~1.16 giây

---

<!-- Strategy 3 decryption removed from canonical instructions. -->

---

## 📊 Bước 4: Xuất DC Coefficients (Optional)

Để xem DC coefficients được mã hóa của từng NALU:

### 4.1 Sửa Source Code để Xuất DC

Edit `pipeline_hybrid_s1_fixed.cpp` và thêm debug output:

```cpp
// Sau dòng: Apply encryption
printf("NALU[%d]: Type=%d, Size=%zu bytes\n", nalu_index, nalu_type, nalu_payload.size());
printf("  DC bytes [19-43]: ");
for (int i = 19; i < 44 && i < (int)nalu_payload.size(); i++) {
    printf("%02X ", nalu_payload[i]);
}
printf("\n");
```

### 4.2 Hoặc Tạo Tool Riêng Để Extract DC

```cpp
// extract_dc_from_encrypted.cpp
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./extract_dc <encrypted.h264>\n";
        return 1;
    }
    
    std::ifstream infile(argv[1], std::ios::binary);
    uint8_t buffer[1024 * 1024];
    
    int nalu_count = 0;
    while (infile.read((char*)buffer, sizeof(buffer)) || infile.gcount() > 0) {
        size_t bytes_read = infile.gcount();
        // Parse NALUs và extract DC
        // ...
        nalu_count++;
        if (nalu_count % 100 == 0) {
            printf("Processed %d NALUs...\n", nalu_count);
        }
    }
    
    infile.close();
    printf("Total NALUs: %d\n", nalu_count);
    return 0;
}
```

---

## ✅ Bước 5: Xác Minh Kết Quả

### 5.1 Kiểm Tra NALU Structure (Verify)

```bash
# Kiểm tra file gốc
/tmp/h264bitstream/.builddir/h264_analyze output.h264 | grep "nal_unit_type:" | sort | uniq -c

# Output:
#   18882 Type 1 (P/B frames)
#     155 Type 5 (I-frames)
#       1 Type 6 (SEI)
#     155 Type 7 (SPS)
#     155 Type 8 (PPS)

# Kiểm tra S1 decrypted
/tmp/h264bitstream/.builddir/h264_analyze output.h264.s1_hybrid_fixed.decrypted | grep "nal_unit_type:" | sort | uniq -c
# Kỳ vọng: GIỐNG HỆT file gốc ✅
```

### 5.2 So Sánh File Size

```bash
ls -lh output.h264*
# output.h264                    219M  ← Original
# output.h264.s1_hybrid_fixed    219M  ← Encrypted S1 (same size)
# output.h264.s1_hybrid_fixed.decrypted  219M  ← Decrypted (should match original)
```

### 5.3 Binary Compare (MD5)

```bash
md5sum output.h264 output.h264.s1_hybrid_fixed.decrypted

# Ghi chú: Decrypted file có thể không giống byte-to-byte do emulation prevention bytes,
# nhưng NALU structure phải giống hệt
```

---

## 🔧 Bước 6: Chạy Toàn Bộ Pipeline

### 6.1 Full Workflow S1

```bash
#!/bin/bash
VIDEO="output.h264"
PASSWORD="testkey"

echo "=== S1 Full Pipeline ==="

# Compile
echo "1. Compiling..."
g++ -std=c++11 -O2 -o pipeline_hybrid_s1_fixed \
    pipeline_hybrid_s1_fixed.cpp \
    hybrid_encryption.cpp \
    dc_metadata.cpp
g++ -std=c++11 -O2 -o pipeline_hybrid_decrypt_s1_h264analyze \
  pipeline_hybrid_decrypt_s1_h264analyze.cpp \
  hybrid_encryption.cpp \
  dc_metadata.cpp

# Encrypt
echo "2. Encrypting..."
time ./pipeline_hybrid_s1_fixed "$VIDEO" "$PASSWORD"

# Decrypt
echo "3. Decrypting..."
time ./pipeline_hybrid_decrypt_s1_h264analyze "${VIDEO}.s1_hybrid_fixed" "$PASSWORD" nalu_info.bin

# Verify
echo "4. Verifying..."
/tmp/h264bitstream/.builddir/h264_analyze "${VIDEO}.s1_hybrid_fixed.decrypted" | grep "nal_unit_type:" | sort | uniq -c > /tmp/s1_result.txt
/tmp/h264bitstream/.builddir/h264_analyze "$VIDEO" | grep "nal_unit_type:" | sort | uniq -c > /tmp/original.txt
diff /tmp/original.txt /tmp/s1_result.txt && echo "✅ NALU structure matches!" || echo "❌ NALU structure differs!"
```

### 6.2 Chạy Tất Cả Strategies (S1, S2)

```bash
#!/bin/bash
VIDEO="output.h264"
PASSWORD="testkey"

for STRATEGY in s1 s2; do
    echo "========== $STRATEGY =========="
    
    # Compile
    g++ -std=c++11 -O2 -o pipeline_hybrid_${STRATEGY}_fixed \
      pipeline_hybrid_${STRATEGY}_fixed.cpp \
      hybrid_encryption.cpp \
      dc_metadata.cpp
    g++ -std=c++11 -O2 -o pipeline_hybrid_decrypt_${STRATEGY}_h264analyze \
      pipeline_hybrid_decrypt_${STRATEGY}_h264analyze.cpp \
      hybrid_encryption.cpp \
      dc_metadata.cpp
    
    # Encrypt
    echo "Encrypting ${STRATEGY}..."
    time ./pipeline_hybrid_${STRATEGY}_fixed "$VIDEO" "$PASSWORD"
    
    # Decrypt
    echo "Decrypting ${STRATEGY}..."
    time ./pipeline_hybrid_decrypt_${STRATEGY}_h264analyze "${VIDEO}.${STRATEGY}_hybrid_fixed" "$PASSWORD" nalu_info.bin
    
    # Verify
    echo "Verifying ${STRATEGY}..."
    /tmp/h264bitstream/.builddir/h264_analyze "${VIDEO}.${STRATEGY}_hybrid_fixed.decrypted" | \
        grep "nal_unit_type:" | sort | uniq -c
    
    echo ""
done
```

---

## 📈 So Sánh Các Strategy

### Hiệu Suất

```
S1 (Mã hóa TẤT CẢ):
  └─ Encrypt: 1.37s
  └─ Decrypt: 1.43s
  └─ Total: 2.80s
  └─ Frames: 18,202

S2 (I-frames only):
  └─ Encrypt: 1.18s
  └─ Decrypt: 1.16s
  └─ Total: 2.34s
  └─ Frames: 148

<!-- S3 removed from project; historical S3 artifacts are archived in `s3_artifacts/` -->
```

### Bảo Mật vs Tốc Độ

```
┌─────────┬──────────┬──────────┬──────────┐
│Strategy │  Tốc độ  │  Bảo mật │  Khuyến  │
├─────────┼──────────┼──────────┼──────────┤
│   S1    │   Chậm   │   Cao    │ Live HD  │
│   S2    │   Nhanh  │   Thấp   │ Preview  │
│   S3    │   N/A    │   N/A    │ Archived │
└─────────┴──────────┴──────────┴──────────┘
```

---

## 🎯 Ví Dụ Chạy Chi Tiết

### Example 1: Chạy S1 Từ Đầu

```bash
cd /home/minh/Documents/NCKH20262/bitstream_impl_arnold2d

# Step 1: Compile
echo "Step 1: Compiling S1..."
g++ -std=c++11 -O2 -o pipeline_hybrid_s1_fixed \
    pipeline_hybrid_s1_fixed.cpp \
    hybrid_encryption.cpp \
    dc_metadata.cpp
echo "✅ Compiled: pipeline_hybrid_s1_fixed"

# Step 2: Encrypt
echo ""
echo "Step 2: Encrypting with password 'testkey'..."
time ./pipeline_hybrid_s1_fixed output.h264 testkey
echo "✅ Generated:"
echo "   - output.h264.s1_hybrid_fixed (219 MB)"
echo "   - output.h264.s1_hybrid_fixed.meta (72 KB)"

# Step 3: Compile decrypt
echo ""
echo "Step 3: Compiling S1 decrypt..."
g++ -std=c++11 -O2 -o pipeline_hybrid_decrypt_s1_h264analyze \
  pipeline_hybrid_decrypt_s1_h264analyze.cpp \
  hybrid_encryption.cpp \
  dc_metadata.cpp
echo "✅ Compiled: pipeline_hybrid_decrypt_s1_h264analyze"

# Step 4: Decrypt
echo ""
echo "Step 4: Decrypting with same password..."
time ./pipeline_hybrid_decrypt_s1_h264analyze output.h264.s1_hybrid_fixed testkey nalu_info.bin
echo "✅ Generated:"
echo "   - output.h264.s1_hybrid_fixed.decrypted (219 MB)"

# Step 5: Verify
echo ""
echo "Step 5: Verifying NALU structure..."
echo "Original file NALU types:"
/tmp/h264bitstream/.builddir/h264_analyze output.h264 | grep "nal_unit_type:" | sort | uniq -c

echo ""
echo "Decrypted file NALU types:"
/tmp/h264bitstream/.builddir/h264_analyze output.h264.s1_hybrid_fixed.decrypted | grep "nal_unit_type:" | sort | uniq -c

echo ""
echo "✅ If counts match → Encryption/Decryption successful!"
```

### Example 2: Chạy S2 (I-frames only)

```bash
cd /home/minh/Documents/NCKH20262/bitstream_impl_arnold2d

# Compile
g++ -std=c++11 -O2 -o pipeline_hybrid_s2_fixed \
    pipeline_hybrid_s2_fixed.cpp \
    hybrid_encryption.cpp \
    dc_metadata.cpp

# Compile decrypt (h264analyze variant)
g++ -std=c++11 -O2 -o pipeline_hybrid_decrypt_s2_h264analyze \
  pipeline_hybrid_decrypt_s2_h264analyze.cpp \
  hybrid_encryption.cpp \
  dc_metadata.cpp

# Encrypt only I-frames
./pipeline_hybrid_s2_fixed output.h264 mypassword

# Decrypt
./pipeline_hybrid_decrypt_s2_h264analyze output.h264.s2_hybrid_fixed mypassword nalu_info.bin

# Verify
/tmp/h264bitstream/.builddir/h264_analyze output.h264.s2_hybrid_fixed.decrypted | grep "nal_unit_type:" | sort | uniq -c
```

---

## ⚠️ Lưu Ý Quan Trọng

1. **Password phải chính xác**: Encrypt và decrypt phải dùng cùng password
2. **Metadata file bắt buộc**: File `.meta` phải có cùng thư mục khi giải mã
3. **NALU structure phải match**: Số lượng và type của mỗi NALU phải giống file gốc
4. **DC offset cố định**: DC được lấy từ byte 19-43 (25 bytes)
5. **File size không đổi**: Encrypted file size = Original file size (format-preserving)

---

## 🆘 Troubleshooting

### Lỗi: "Metadata file not found"
```
Nguyên nhân: File .meta không ở cùng thư mục
Giải pháp: Đảm bảo cả 2 file ở cùng folder
  - output.h264.s1_hybrid_fixed
  - output.h264.s1_hybrid_fixed.meta
```

### Lỗi: "NALU structure mismatch"
```
Nguyên nhân: Password sai hoặc corruption
Giải pháp: 
  1. Dùng cùng password
  2. Kiểm tra file không bị corrupt
  3. Chạy lại từ đầu
```

### Lỗi: "h264_analyze command not found"
```
Giải pháp: Build h264bitstream
  cd /tmp
  git clone https://github.com/seanmcleod/h264bitstream.git
  cd h264bitstream
  ./configure && make
```

---

## 📝 Summary

| Bước | Lệnh | Kết quả |
|------|------|--------|
| **Compile** | `g++ -o pipeline_hybrid_s*_fixed ...` | 6 executables |
| **Encrypt S1** | `./pipeline_hybrid_s1_fixed input.h264 pwd` | `.s1_hybrid_fixed` + `.meta` |
| **Decrypt S1** | `./pipeline_hybrid_decrypt_s1_h264analyze input.h264.s1_hybrid_fixed pwd nalu_info.bin` | `.decrypted` |
| **Verify** | `h264_analyze file \| grep "nal_unit_type"` | NALU structure check |

✅ **Sẵn sàng để chạy!**
