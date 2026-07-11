# Hướng dẫn đo metrics — measure_metrics.py

## Mục đích

So sánh 4 thuật toán mã hoá DC H.264 (hybrid, AES, DES, RC4) trên các nhóm chỉ số:

| Nhóm | Chỉ số | Mục tiêu |
|------|--------|----------|
| Security | NPCR, UACI, Entropy, Byte Correlation | NPCR > 99.6%, Entropy > 7.9 |
| Visual Quality | PSNR, SSIM, TSSIM (pixel + DC domain) | PSNR thấp, SSIM ≈ 0 (đã mã hoá) |
| Correctness | SHA256, BER, Codec Compliance | BER = 0, SHA256 MATCH |
| Performance | DC encrypt time, ns/NALU | Càng thấp càng tốt |

---

## Yêu cầu

```bash
pip install numpy scipy scikit-image opencv-python
```
Ngoài ra cần `ffmpeg` và `ffprobe` trong PATH (để đọc frame và đo FPS).

---

## Cách chạy — từng bước

### Bước 0: Chuẩn bị file encrypt/decrypt

```bash
# Đảm bảo đã chạy extract và encrypt/decrypt cho từng algo trước
./extract/extract_nalu_from_h264analyze videos/output.h264

for ALGO in hybrid aes des rc4; do
    ./pipelines/pipeline_hybrid_s1_h264analyze videos/output.h264 mykey --algo $ALGO
    ./pipelines/pipeline_hybrid_decrypt_s1_h264analyze \
        results/output.h264/${ALGO}_s1/output.h264.s1_${ALGO}* mykey --algo $ALGO
done
```

### Bước 1: Đo từng algo

```bash
# Hybrid
python3 scripts/measure_metrics.py \
    --original  videos/output.h264 \
    --encrypted results/output.h264/hybrid_s1/output.h264.s1_hybrid_fixed \
    --decrypted results/output.h264/hybrid_s1/output.h264.s1_hybrid_fixed.decrypted \
    --strategy S1 --algo hybrid --key mykey

# AES
python3 scripts/measure_metrics.py \
    --original  videos/output.h264 \
    --encrypted results/output.h264/aes_s1/output.h264.s1_aes \
    --decrypted results/output.h264/aes_s1/output.h264.s1_aes.decrypted \
    --strategy S1 --algo aes --key mykey

# DES
python3 scripts/measure_metrics.py \
    --original  videos/output.h264 \
    --encrypted results/output.h264/des_s1/output.h264.s1_des \
    --decrypted results/output.h264/des_s1/output.h264.s1_des.decrypted \
    --strategy S1 --algo des --key mykey

# RC4
python3 scripts/measure_metrics.py \
    --original  videos/output.h264 \
    --encrypted results/output.h264/rc4_s1/output.h264.s1_rc4 \
    --decrypted results/output.h264/rc4_s1/output.h264.s1_rc4.decrypted \
    --strategy S1 --algo rc4 --key mykey
```

### Bước 2 (tuỳ chọn): Đo key sensitivity

Tạo file encrypted với key lệch 1 ký tự, rồi truyền vào `--encrypted2`:

```bash
./pipelines/pipeline_hybrid_s1_h264analyze videos/output.h264 mykey2 --algo hybrid
python3 scripts/measure_metrics.py \
    --original  videos/output.h264 \
    --encrypted results/output.h264/hybrid_s1/output.h264.s1_hybrid_fixed \
    --strategy S1 --algo hybrid --key mykey \
    --encrypted2 results/output.h264/hybrid_s1/output.h264.s1_hybrid_fixed
    # (thay --encrypted2 bằng file encrypt với key khác)
```

### Bước 3 (tuỳ chọn): Test wrong-key

```bash
python3 scripts/measure_metrics.py \
    --original  videos/output.h264 \
    --encrypted results/output.h264/hybrid_s1/output.h264.s1_hybrid_fixed \
    --decrypted results/output.h264/hybrid_s1/output.h264.s1_hybrid_fixed.decrypted \
    --strategy S1 --algo hybrid --key mykey --wrong-key wrongkey
```

---

## Tham số đầy đủ

| Tham số | Mặc định | Mô tả |
|---------|----------|-------|
| `--original` | *(bắt buộc)* | File H.264 gốc |
| `--encrypted` | *(bắt buộc)* | File H.264 đã mã hoá |
| `--decrypted` | None | File H.264 đã giải mã |
| `--strategy` | S1 | S1 (Type1+Type5) hoặc S2 (Type5 only) |
| `--algo` | hybrid | hybrid / aes / des / rc4 |
| `--key` | None | Key đã dùng để encrypt (cần cho timing + plaintext sensitivity) |
| `--wrong-key` | None | Key sai để test wrong-key scenario |
| `--encrypted2` | None | File encrypt với key khác → NPCR/UACI key sensitivity |
| `--nalu-info` | auto | Path tới `.nalu_info.bin` (tự derive từ `nalu_cache/<video>.nalu_info.bin`) |
| `--timing-runs` | 1 | Số lần chạy lấy trung bình timing (3 để chính xác hơn) |
| `--max-frames` | 60 | Số frame tối đa cho visual metrics |

---

## Output

- **Console**: in đủ tất cả metrics theo từng nhóm
- **File**: `metrics_report_<S1|S2>_<YYYYMMDD_HHMMSS>.txt` tại thư mục hiện tại

---

## Lưu ý

- **Plaintext sensitivity** (Wu et al. 2011) chỉ tính được với `--algo hybrid` vì cần Python reimplementation dùng `nalu_index`. AES/DES/RC4 bỏ qua section này.
- **Visual metrics** (PSNR/SSIM frame) cần FFmpeg decode được file encrypted — với file đã mã hoá DC, FFmpeg thường báo lỗi. Đây là expected behavior, không phải bug.
- **DC domain metrics** (PSNR DC, SSIM DC, Byte Correlation) là chỉ số chính — đo trực tiếp trên bitstream, không cần decode.
- **Timing** đo `DC_ONLY_TIME_MS` (thời gian thuần encrypt/decrypt array DC, không tính I/O) lấy từ stdout của C++ pipeline.
