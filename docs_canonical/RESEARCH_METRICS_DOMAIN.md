# Nghiên Cứu: Domain Đo Metrics Cho Selective H.264 Encryption
**Ngày:** 2026-06-16  
**Câu hỏi:** Trong literature, NPCR/UACI/Entropy/Correlation đo ở pixel domain hay bitstream domain?

---

## Tóm Tắt Phát Hiện

Qua tìm kiếm trên 6 paper chính thống về selective H.264/HEVC/VVC encryption:

> **Không có paper nào đo NPCR/UACI thuần túy trên raw bitstream bytes.**  
> Tất cả đều đo ở **pixel domain** (decoded frames). Tuy nhiên, họ có lý do chính đáng để kết quả đạt ngưỡng, và có một nhánh phụ dùng **bitstream entropy** làm primary metric.

---

## Các Paper Tìm Được

### 1. Selective Encryption of the Versatile Video Coding (VVC) Standard
- **Tác giả:** Peng, Boyadjis, Bergeron et al.
- **Venue:** 2021, IEEE TCSVT (preprint arXiv:2103.04203)
- **URL:** https://arxiv.org/pdf/2103.04203
- **Domain đo:** Pixel domain — NPCR, UACI đo trên decoded frames
- **Cách xử lý:** Encrypt **syntax elements** (TC values, MV differences, sign bits) tại tầng CABAC. Decoder nhận syntax lỗi → tạo **noise thay vì gray frame** → NPCR/UACI đạt ngưỡng chuẩn

---

### 2. Enhancing Selective Encryption for H.264/AVC Using AES
- **Venue:** 2022, arXiv:2201.03391
- **URL:** https://arxiv.org/pdf/2201.03391
- **Domain đo:** Pixel domain — NPCR, UACI, pixel correlation
- **Cách xử lý:** Encrypt codewords intra prediction modes, MVD, sign bits DCT. Các elements này có **error propagation mạnh** — một bit sai lan toàn frame → NPCR ≈ 99%

---

### 3. Confidentiality of a Selectively Encrypted H.264 Coded Video Bit-stream
- **Tác giả:** Asghar, Ghanbari, Fleury, Reed
- **Venue:** 2014, Journal of Visual Communication and Image Representation (Elsevier), vol. 25, no. 2, pp. 487–498
- **DOI:** https://doi.org/10.1016/j.jvcir.2013.11.001
- **Domain đo:** Pixel domain — phân tích frame từ góc nhìn cryptanalysis
- **Cách xử lý:** Không chỉ đo NPCR/UACI mà còn phân tích khả năng attacker **dùng error concealment để recover video** — đây là cách tiếp cận độc đáo nhất trong danh sách

---

### 4. Extended Selective Encryption of H.264/AVC (CABAC) and HEVC
- **Tác giả:** Boyadjis, Bergeron, Pesquet-Popescu, Dufaux
- **Venue:** 2017, IEEE Transactions on Circuits and Systems for Video Technology, vol. 27, no. 4, pp. 892–906
- **URL:** https://hal.science/hal-01433748/file/2017_TCSVT_Boyadjis_el_al.pdf
- **Domain đo:** Pixel domain — visual security trên decoded output
- **Cách xử lý:** Encrypt **đủ số lượng syntax elements** để error propagation lan rộng toàn frame. Nguyên tắc: chọn elements có **error propagation effect mạnh nhất**

---

### 5. Lightweight Cipher for H.264 Videos in the Internet of Multimedia Things
- **Venue:** 2019, MDPI Sensors, PMC6427165
- **URL:** https://pmc.ncbi.nlm.nih.gov/articles/PMC6427165/
- **Domain đo:** Pixel domain — pixel correlation (H/V/D), entropy, NPCR, UACI
- **Đặc biệt:** Thêm metric **Encryption Space Ratio** (tỷ lệ % bytes được encrypt trong bitstream) làm secondary metric — đây là metric ở bitstream domain duy nhất được đề cập

---

### 6. Design and ARM-Based Implementation of Bitstream-Oriented Chaotic Encryption for H.264/AVC
- **Venue:** 2021, PMC8621351
- **URL:** https://pmc.ncbi.nlm.nih.gov/articles/PMC8621351/
- **Domain đo:** **Song song bitstream + pixel domain** — paper gần nhất với bitstream domain
- **Đặc biệt:** Đo **entropy của byte sequence bitstream** trước/sau encrypt làm primary security metric, kết hợp với pixel-domain metrics. Bitstream entropy được coi là metric độc lập, không bị ảnh hưởng bởi error concealment

---

## Phân Tích: Tại Sao Các Paper Dùng Pixel Domain Mà Vẫn Đạt Ngưỡng?

Điểm then chốt mà researcher phát hiện:

> **Họ encrypt các syntax elements có error propagation mạnh** (intra prediction modes, MVD, sign bits DCT), không phải DC coefficient values.

Khi một **intra prediction mode** bị scramble, toàn bộ block không thể reconstruct → lỗi lan sang block liền kề → lan toàn frame → `decoded frame = noise`. Lúc này:
- Noise frame ≠ gray frame → std ≠ 0 → NPCR/Correlation tính được
- Noise frame vs original → NPCR ≈ 99.6%, UACI ≈ 33.46%

Ngược lại, dự án này encrypt **DC coefficient values** trong payload NALU. Đây là giá trị nội dung, không phải syntax structure. Decoder vẫn parse được bitstream → kích hoạt error concealment → gray frame.

---

## So Sánh Hai Hướng Tiếp Cận

| Tiêu chí | Pixel Domain (mainstream) | Bitstream Domain |
|----------|--------------------------|-----------------|
| Được dùng trong paper | Rất phổ biến | Ít (PMC8621351, một số chaotic cipher) |
| Có thể đạt ngưỡng NPCR/UACI | Có (nếu encrypt syntax elements) | Không áp dụng (NPCR/UACI là pixel concept) |
| Phù hợp với dự án DC encryption | **Không** — gray frame problem | **Có** — đo trực tiếp bytes đã encrypt |
| Metrics chuẩn | NPCR, UACI, Entropy, Correlation | Bitstream Entropy, Encryption Space Ratio, Byte Correlation |
| Chấp nhận khi publish | Cần justify | Cần cite PMC8621351 và giải thích |

---

## Recommendation Từ Researcher Agent

Dựa trên literature, có **hai hướng giải quyết**:

### Hướng A — Đo Bitstream Domain (phù hợp kiến trúc DC encryption)
Dùng **bitstream entropy + Encryption Space Ratio** làm primary metrics:

```
Bitstream Entropy (bytes 19–43 của NALU):
  - Trước encrypt: entropy của DC values gốc
  - Sau encrypt: entropy của DC values sau XOR → kỳ vọng ~7.9+ bits/byte
  
Encryption Space Ratio:
  - = (số bytes được encrypt) / (tổng bytes bitstream) × 100%
  - S1: 19037 × 25 / 228,618,587 ≈ 2.08%
  - S2: 155 × 25 / 228,618,587 ≈ 0.017%

Byte Correlation (thay Pixel Correlation):
  - Pearson correlation giữa original DC bytes và encrypted DC bytes
  - Kỳ vọng: ≈ 0 nếu XOR keystream đủ ngẫu nhiên
```

**Cite:** PMC8621351 (2021) — paper duy nhất đo bitstream entropy như primary security metric.

### Hướng B — Sửa Pixel Domain bằng `-ec 0`
Dùng FFmpeg với error concealment disabled:
```bash
ffmpeg -v error -flags2 +showall -ec 0 -i encrypted.h264 -f rawvideo -pix_fmt gray pipe:1
```
Không có error concealment → decoder xuất **noise thực sự** thay vì gray frame → NPCR/UACI có thể tính được.

**Lưu ý:** Kết quả phụ thuộc vào cách FFmpeg xử lý khi `-ec 0`, có thể vẫn nhận được partial frames hoặc crash tùy cấu trúc NALU.

### Kết Luận

**Khuyến nghị cho dự án này:** Dùng **Hướng A (Bitstream Domain)** vì:
1. Chính xác và trung thực hơn với kiến trúc DC encryption
2. Tránh hoàn toàn vấn đề gray-frame
3. Có precedent trong PMC8621351 (2021)
4. Thêm Encryption Space Ratio làm rõ tỷ lệ dữ liệu được bảo vệ

Giữ PSNR/SSIM pixel-domain (nhóm 2) nhưng ghi rõ: *"đo sau error concealment — phản ánh mức độ ẩn nội dung với decoder thông thường, không phải encryption strength"*.

---

*Nghiên cứu bởi researcher agent — 2026-06-16*
