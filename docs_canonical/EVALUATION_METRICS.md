# Báo Cáo: Bộ Metrics Đánh Giá Hệ Thống Mã Hoá DC Coefficients H.264

**Ngày:** 2026-06-16
**Dự án:** Selective Encryption of DC Coefficients in H.264 Bitstream (S1/S2)
**Thuật toán:** PLCM keystream + Arnold 2D permutation (5×5, P=3, Q=11) + XOR diffusion

---

## Tổng Quan

Hệ thống mã hoá có chọn lọc bytes 19–43 (hệ số DC) trong từng NALU VCL của H.264, không re-encode. Cần đánh giá theo 4 chiều: bảo mật, chất lượng video, hiệu năng và tính đúng đắn của decrypt.

---

## Nhóm 1: Bảo Mật (Security Metrics)

### 1. Keyspace Size

- **Mô tả:** Tổng số key hợp lệ của hệ thống
- **Ngưỡng tham chiếu:** >= 2^128 (chống brute-force)
- **Cách đo:** Phân tích lý thuyết tổ hợp: PLCM (floating-point 64-bit) × Arnold parameters × iteration count
- **Ý nghĩa với dự án:** Cần liệt kê tổng keyspace và chứng minh vượt ngưỡng 2^128

### 2. NPCR — Number of Pixels Change Rate

- **Mô tả:** Tỉ lệ pixels thay đổi khi đổi 1 bit trong key
- **Công thức:** `NPCR = (∑ D(i,j) / (W×H)) × 100%` trong đó D(i,j)=1 nếu pixel khác nhau
- **Ngưỡng tham chiếu:** > 99.6%
- **Ý nghĩa:** Chống differential attack — đặc biệt quan trọng vì hệ thống dùng XOR diffusion

### 3. UACI — Unified Average Changing Intensity

- **Mô tả:** Cường độ thay đổi trung bình giữa hai ciphertext khi đổi 1 bit key
- **Công thức:** `UACI = (∑ |C1(i,j) - C2(i,j)| / (255 × W×H)) × 100%`
- **Ngưỡng tham chiếu:** ~33.46% (lý thuyết tối ưu cho 8-bit)
- **Ý nghĩa:** Kết hợp với NPCR để đánh giá toàn diện key sensitivity

### 4. Information Entropy

- **Mô tả:** Độ ngẫu nhiên của ciphertext output
- **Công thức:** `H = -∑ p(i) × log2(p(i))` với i = 0..255
- **Ngưỡng tham chiếu:** > 7.9 / 8.0 (lý tưởng)
- **Ý nghĩa:** Entropy thấp → ciphertext còn pattern → dễ phân tích thống kê

### 5. Adjacent Pixel Correlation Coefficient

- **Mô tả:** Hệ số tương quan giữa các pixel liền kề (horizontal/vertical/diagonal)
- **Công thức:** Pearson correlation giữa pixel(x,y) và pixel(x+1,y), pixel(x,y+1), pixel(x+1,y+1)
- **Ngưỡng tham chiếu:** < 0.05 sau mã hoá (gốc thường > 0.9)
- **Ý nghĩa:** Correlation cao → spatial structure chưa bị phá vỡ → mã hoá yếu

---

## Nhóm 2: Chất Lượng Video Sau Mã Hoá (Visual Quality Metrics)

### 6. PSNR — Peak Signal-to-Noise Ratio

- **Mô tả:** So sánh chất lượng frame gốc vs frame đã mã hoá
- **Công thức:** `PSNR = 10 × log10(255² / MSE)` (dB)
- **Ngưỡng tham chiếu:** Giá trị càng thấp = mã hoá càng hiệu quả (< 10 dB với S1)
- **Đo riêng theo:** Frame I, Frame P, Frame B để phân biệt ảnh hưởng S1 vs S2
- **Ý nghĩa:** Metric cơ bản nhất, dễ so sánh giữa các paper

### 7. SSIM — Structural Similarity Index

- **Mô tả:** Đánh giá tương đồng cấu trúc theo nhận thức thị giác con người
- **Công thức:** Tổ hợp luminance, contrast, structure similarity
- **Ngưỡng tham chiếu:** Gần 0 = mã hoá tốt, gần 1 = ít ảnh hưởng
- **Ý nghĩa:** Nhạy hơn PSNR, phản ánh chính xác hơn cảm nhận của người xem

### 8. TSSIM — Temporal SSIM

- **Mô tả:** SSIM theo thời gian, đo flickering và artifact lan truyền giữa các frame liên tiếp
- **Công thức:** SSIM giữa frame(t) và frame(t+1) của video encrypted
- **Ngưỡng tham chiếu:** Càng thấp = temporal distortion càng rõ
- **Ý nghĩa với S2:** Với S2 (I-frame only), lỗi DC từ I-frame lan truyền sang P/B frame qua motion compensation → TSSIM capture artifact này, PSNR per-frame bỏ sót

---

## Nhóm 3: Hiệu Năng (Performance Metrics)

### 9. Encryption/Decryption Throughput

- **Mô tả:** Tốc độ xử lý mã hoá/giải mã
- **Đơn vị:** fps (frames per second) hoặc MB/s
- **Ngưỡng tham chiếu:** >= 25 fps với HD 720p (real-time threshold)
- **Đo riêng:** S1 throughput vs S2 throughput
- **Overhead ratio:** `(Time_encrypt - Time_passthrough) / Time_passthrough` — dự kiến < 5% với in-bitstream approach

---

## Nhóm 4: Tính Đúng Đắn Của Decrypt (Correctness Metrics)

### 10. BER + PSNR=∞ + Codec Compliance

**a) BER — Bit Error Rate**
- **Yêu cầu:** BER = 0 sau decrypt với đúng key
- **Cách đo:** `cmp --silent original.h264 decrypted.h264`

**b) PSNR sau decrypt**
- **Yêu cầu:** PSNR = ∞ (MSE = 0) — byte-exact recovery
- **Cách đo:** SHA256 hash so sánh hoặc diff bitstream

**c) Codec Compliance Test**
- **Yêu cầu:** File đã mã hoá vẫn decode được bằng FFmpeg/JM decoder (format-compliant)
- **Ý nghĩa:** Chứng minh đặc tính cốt lõi của dự án — mã hoá in-bitstream, bitrate overhead = 0%

**d) Wrong-Key Test**
- **Yêu cầu:** Với sai key, output phải unviewable (PSNR thấp, unrecognizable)
- **Mục đích:** Xác nhận không có information leakage khi không có key

---

## Bảng Tổng Hợp

| # | Metric | Nhóm | Ngưỡng | Ưu tiên |
|---|--------|-------|--------|---------|
| 1 | Keyspace size | Security | >= 2^128 | Cao |
| 2 | NPCR | Security | > 99.6% | Cao |
| 3 | UACI | Security | ~33.46% | Cao |
| 4 | Information Entropy | Security | > 7.9 | Trung bình |
| 5 | Pixel Correlation | Security | < 0.05 | Trung bình |
| 6 | PSNR (encrypted) | Visual | < 10 dB (S1) | Cao |
| 7 | SSIM | Visual | Gần 0 | Cao |
| 8 | TSSIM | Visual | Càng thấp càng tốt | Cao (S1 vs S2) |
| 9 | Throughput (fps) | Performance | >= 25 fps | Trung bình |
| 10a | BER sau decrypt | Correctness | = 0 | Bắt buộc |
| 10b | PSNR sau decrypt | Correctness | = ∞ | Bắt buộc |
| 10c | Codec compliance | Correctness | Pass | Bắt buộc |
| 10d | Wrong-key test | Correctness | PSNR thấp | Cao |

---

## Tài Liệu Tham Khảo

1. Z. Shahid, M. Chaumont, W. Puech — *Fast Protection of H.264/AVC by Selective Encryption of CAVLC and CABAC*, IEEE TCSVT 2011
2. IEEE #4453841 — Paper gốc do người dùng cung cấp
3. PMC6427165 — *Lightweight Cipher for H.264 with Encryption Space Ratio Diagnostics*
4. Springer SIVP 2023 — *A fast selective encryption scheme for H.264/AVC*
5. arXiv 2302.07411 — *Real-time chaotic video encryption based on multithreaded parallel confusion and diffusion*
6. ResearchGate — *A Format-Compliant Selective Encryption Scheme for Real-Time Video Streaming of H.264/AVC*
