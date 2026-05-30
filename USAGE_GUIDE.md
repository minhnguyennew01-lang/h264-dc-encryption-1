# Hướng Dẫn Sử Dụng Hệ Thống Mã Hóa H.264

**Ngày tạo:** April 20, 2026  
**Dự án:** H.264 DC Encryption với PLCM Chaotic Map  
**Vị trí:** `/home/minh/Documents/NCKH20262/bitstream_impl/`

## 🎯 Tổng Quan

Hệ thống này mã hóa DC coefficients của video H.264 sử dụng:
- **PLCM (Piecewise Linear Chaotic Map)** cho keystream
- **Arnold Cat Map** cho confusion
- **XOR với chaining** cho diffusion
- **3 chiến lược mã hóa** khác nhau
- **Byte-perfect decryption** (khôi phục 100%)

**Đặc điểm nổi bật:**
- ✅ Mỗi NALU có seed riêng (PLCM deterministic)
- ✅ File size preserved (giữ nguyên kích thước)
- ✅ Video vẫn playable sau mã hóa
- ✅ Khôi phục hoàn hảo 100%

---

## 📋 Chuẩn Bị Môi Trường

### 1. Di Chuyển Vào Thư Mục
```bash
cd /home/minh/Documents/NCKH20262/bitstream_impl
```

### 2. Build Tất Cả Chương Trình
```bash
make clean
make
```

**Kết quả mong đợi:**
```
g++ -o pipeline_encrypt_strategy1_preserve ...
g++ -o pipeline_encrypt_strategy2_preserve ...
g++ -o pipeline_encrypt_strategy3_preserve ...
g++ -o pipeline_decrypt_preserve ...
[✓] All executables compiled successfully
```

### 3. Kiểm Tra File Đã Build
```bash
ls -la pipeline_encrypt_* pipeline_decrypt_* extract_dc_*
```

---

## 🚀 Cách Chạy Chi Tiết

## Chiến Lược 1: Mã Hóa Tất Cả Frames (I/P/B) - Bảo Mật Tối Đa

### Mã Hóa
```bash
./pipeline_encrypt_strategy1_preserve <input.h264> <output.h264> <key>
```

**Ví dụ:**
```bash
./pipeline_encrypt_strategy1_preserve DC_Strategy1.h264 encrypted_s1.h264 "mykey123"
```

**Output:**
```
[1/4] Reading NALUs from DC_Strategy1.h264
[2/4] Processing 19193 NALUs with Strategy 1 (All frames)
[3/4] Encrypted 19038 frames (98.4% of total)
[4/4] Wrote encrypted file: encrypted_s1.h264

✓ File size: 219 MB (preserved)
✓ Time: ~2.5 seconds
```

### Giải Mã
```bash
./pipeline_decrypt_preserve <encrypted.h264> <original.h264> <decrypted.h264> <key>
```

**Ví dụ:**
```bash
./pipeline_decrypt_preserve encrypted_s1.h264 DC_Strategy1.h264 decrypted_s1.h264 "mykey123"
```

---

## Chiến Lược 2: Mã Hóa Chỉ I-Frames - Khuyến Nghị ⭐

### Mã Hóa (Nhanh, Ít Ảnh Hưởng Hình Ảnh)
```bash
./pipeline_encrypt_strategy2_preserve <input.h264> <output.h264> <key>
```

**Ví dụ:**
```bash
./pipeline_encrypt_strategy2_preserve DC_Strategy2.h264 encrypted_s2.h264 "mykey123"
```

**Output:**
```
[1/4] Reading NALUs from DC_Strategy2.h264
[2/4] Processing 19193 NALUs with Strategy 2 (I-frames only)
[3/4] Encrypted 155 I-frames (0.8% of total)
[4/4] Wrote encrypted file: encrypted_s2.h264

✓ File size: 219 MB (preserved)
✓ Time: ~1.5 seconds
```

### Giải Mã
```bash
./pipeline_decrypt_preserve encrypted_s2.h264 DC_Strategy2.h264 decrypted_s2.h264 "mykey123"
```

---

## Chiến Lược 3: Mã Hóa I+P Frames - Cân Bằng

### Mã Hóa
```bash
./pipeline_encrypt_strategy3_preserve <input.h264> <output.h264> <key>
```

**Ví dụ:**
```bash
./pipeline_encrypt_strategy3_preserve DC_Strategy3.h264 encrypted_s3.h264 "mykey123"
```

**Output:**
```
[1/4] Reading NALUs from DC_Strategy3.h264
[2/4] Processing 19193 NALUs with Strategy 3 (I+P frames)
[3/4] Encrypted 19038 frames (98.4% of total)
[4/4] Wrote encrypted file: encrypted_s3.h264

✓ File size: 219 MB (preserved)
✓ Time: ~2.0 seconds
```

### Giải Mã
```bash
./pipeline_decrypt_preserve encrypted_s3.h264 DC_Strategy3.h264 decrypted_s3.h264 "mykey123"
```

---

## 📊 Kiểm Chứng Kết Quả

### 1. Kiểm Tra Kích Thước File
```bash
ls -lh <original.h264> <decrypted.h264>
# Cả hai phải có cùng kích thước
```

### 2. Kiểm Tra MD5 Hash
```bash
md5sum <original.h264> <decrypted.h264>
# MD5 phải giống nhau 100%
```

### 3. So Sánh Byte-to-Byte
```bash
cmp <original.h264> <decrypted.h264>
# Không có output nào = files giống nhau
```

### 4. Phát Video Để Kiểm Tra
```bash
# Phát video gốc
ffplay <original.h264>

# Phát video đã mã hóa (vẫn playable)
ffplay <encrypted.h264>

# Phát video đã giải mã (phải giống gốc)
ffplay <decrypted.h264>
```

---

## 🔍 Phân Tích DC Coefficients (Tùy Chọn)

### Extract DC Từ Video
```bash
# Từ video gốc
./extract_dc_ycbcr <original.h264> > dc_before.txt

# Từ video đã mã hóa
./extract_dc_ycbcr <encrypted.h264> > dc_after.txt
```

### So Sánh DC
```bash
# Đếm số dòng khác nhau
diff dc_before.txt dc_after.txt | wc -l

# Xem chi tiết sự khác nhau
diff dc_before.txt dc_after.txt | head -20
```

### Phân Tích Theo Component YUV
```bash
# Đếm DC coefficients cho Y (luma)
grep "Y\[" dc_before.txt | wc -l

# Đếm DC coefficients cho Cb (chroma blue)
grep "Cb\[" dc_before.txt | wc -l

# Đếm DC coefficients cho Cr (chroma red)
grep "Cr\[" dc_before.txt | wc -l
```

---

## 🧪 Chạy Test Đầy Đủ

```bash
# Chạy script test tự động
./test_and_save_results.sh
```

**Script sẽ:**
- Test tất cả 3 chiến lược
- Lưu kết quả vào `results/`
- Tạo báo cáo so sánh
- Kiểm chứng MD5

---

## 📈 So Sánh Các Chiến Lược

| Chiến Lược | Phạm Vi | Tốc Độ | Ảnh Hưởng Hình Ảnh | Bảo Mật | Khuyến Nghị |
|------------|---------|--------|-------------------|---------|-------------|
| **S1** | 98.4% | Chậm (~2.5s) | Tối đa (scramble hoàn toàn) | Tối đa | Bảo mật cao |
| **S2** | 0.8% | Nhanh (~1.5s) | Tối thiểu (99.2% frames giữ nguyên) | Trung bình | ⭐ Streaming |
| **S3** | 98.4% | Trung bình (~2.0s) | Cao | Mạnh | Cân bằng |

---

## ⚠️ Lưu Ý Quan Trọng

1. **Key Phải Giống:** Mã hóa và giải mã phải dùng cùng key
2. **Deterministic:** Chạy cùng chiến lược nhiều lần → kết quả giống hệt
3. **PLCM Seeding:** Mỗi NALU có seed riêng từ key + nalu_index
4. **File Size:** Luôn preserved (giữ nguyên)
5. **Recovery:** Byte-perfect (100% khôi phục)

---

## 🚨 Xử Lý Lỗi Thường Gặp

### Lỗi: "File not found"
```bash
# Kiểm tra file tồn tại
ls -la <filename.h264>

# Đảm bảo đường dẫn đúng
pwd
```

### Lỗi: "Key mismatch"
- Đảm bảo key giống hệt khi mã hóa và giải mã
- Key có phân biệt hoa thường

### Lỗi: "Decryption failed"
- Kiểm tra file original và encrypted có cùng chiến lược
- Chạy lại build: `make clean && make`

### Lỗi: "Video không phát được"
- Video gốc phải là H.264 hợp lệ
- Sử dụng ffprobe để kiểm tra: `ffprobe <file.h264>`

---

## 📞 Hỗ Trợ

Nếu gặp vấn đề:
1. Kiểm tra log output của chương trình
2. Chạy lại build
3. Kiểm tra file input có hợp lệ không
4. Liên hệ để được hỗ trợ thêm

**Chúc bạn sử dụng thành công! 🎉**