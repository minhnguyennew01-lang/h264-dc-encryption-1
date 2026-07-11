# CLAUDE.md — H.264 DC Selective Encryption (NCKH 2026)

## Tổng quan

Dự án mã hoá **có chọn lọc** các hệ số DC (bytes 19–43 trong mỗi NALU VCL) của file H.264 nhị phân thô, **không re-encode**. Kết quả giải mã phải khớp **byte-exact** với file gốc — đây là invariant cứng, không được vi phạm.

Hai chiến lược:
- **S1**: mã hoá tất cả Type 1 (P/B) + Type 5 (I) frames — 19,037 NALUs
- **S2**: chỉ mã hoá Type 5 (I-frames) — 155 NALUs

Bốn thuật toán (flag `--algo`):
- **hybrid** (default): PLCM keystream + Arnold 2D Cat Map + XOR — keystream khác mỗi NALU (an toàn)
- **aes**: AES-128-CTR với counter cố định — keystream giống mọi NALU (điểm yếu ECB)
- **des**: DES-OFB với counter cố định — keystream giống mọi NALU (điểm yếu)
- **rc4**: RC4 stream cipher, reinit từ đầu mỗi NALU — keystream giống mọi NALU (điểm yếu)

---

## Cấu trúc thư mục

```
bitstream_impl_arnold2d/
├── algorithms/                 # Tất cả thuật toán mã hoá
│   ├── encryption_interface.h  # Abstract base class IEncryption
│   ├── encryption_factory.cpp  # IEncryption::create("hybrid"|"aes"|"des"|"rc4")
│   ├── hybrid_encryption.{h,cpp}  # Core PLCM + Arnold 2D + XOR
│   ├── hybrid_algo.{h,cpp}    # IEncryption wrapper cho hybrid
│   ├── aes_algo.{h,cpp}       # AES-128-CTR (fixed counter)
│   ├── des_algo.{h,cpp}       # DES-OFB (fixed counter)
│   └── rc4_algo.{h,cpp}       # RC4 stream cipher
├── pipelines/                  # Các pipeline encrypt/decrypt
│   ├── pipeline_hybrid_s1_h264analyze.cpp
│   ├── pipeline_hybrid_s2_h264analyze.cpp
│   ├── pipeline_hybrid_decrypt_s1_h264analyze.cpp
│   └── pipeline_hybrid_decrypt_s2_h264analyze.cpp
├── extract/
│   ├── extract_nalu_from_h264analyze.cpp       # Binary: trích xuất NALU info → nalu_cache/
│   └── extract_nalu_from_h264analyze_wrapper.sh # Wrapper: tự tìm/build h264_analyze rồi gọi binary
├── scripts/
│   └── measure_metrics.py      # Đo NPCR/UACI/Entropy (cần update cho --algo)
├── videos/                     # File H.264 đầu vào
├── nalu_cache/                 # Cache NALU offsets: <video>.nalu_info.bin
├── results/                    # Output: results/<video>/<algo>_s<N>/
│   └── output.h264/
│       ├── hybrid_s1/
│       ├── hybrid_s2/
│       ├── aes_s1/  aes_s2/
│       ├── des_s1/  des_s2/
│       └── rc4_s1/  rc4_s2/
├── Makefile
└── old_22_06_2026/             # Files đã bỏ
```

---

## Workflow chính

```bash
# Bước 1: Build tất cả
make

# Bước 2: Trích xuất NALU info (chạy 1 lần mỗi video)
# Lần đầu (chưa build h264_analyze): dùng wrapper — tự tìm và build nếu cần
./extract/extract_nalu_from_h264analyze_wrapper.sh videos/output.h264
# Các lần sau (h264_analyze đã có ở third_party/): dùng binary trực tiếp
./extract/extract_nalu_from_h264analyze videos/output.h264
# → nalu_cache/output.h264.nalu_info.bin

# Bước 3: Encrypt (thay --algo bằng aes/des/rc4/hybrid)
./pipelines/pipeline_hybrid_s1_h264analyze videos/output.h264 <key> --algo hybrid
# → results/output.h264/hybrid_s1/output.h264.s1_hybrid_fixed

# Bước 4: Decrypt
./pipelines/pipeline_hybrid_decrypt_s1_h264analyze \
    results/output.h264/hybrid_s1/output.h264.s1_hybrid_fixed <key> --algo hybrid
# → results/output.h264/hybrid_s1/output.h264.s1_hybrid_fixed.decrypted

# Bước 5: Verify byte-exact
cmp --silent videos/output.h264 \
    results/output.h264/hybrid_s1/output.h264.s1_hybrid_fixed.decrypted \
    && echo 'BYTE-EXACT OK' || echo 'MISMATCH'
```

**Quy tắc bắt buộc**: `nalu_cache/<video>.nalu_info.bin` phải được tạo từ **cùng file input** trước khi encrypt hoặc decrypt.

---

## Đo metrics (measure_metrics.py)

```bash
# Cú pháp chuẩn:
python3 scripts/measure_metrics.py \
    --original  videos/output.h264 \
    --encrypted results/output.h264/hybrid_s1/output.h264.s1_hybrid_fixed \
    --decrypted results/output.h264/hybrid_s1/output.h264.s1_hybrid_fixed.decrypted \
    --strategy S1 \
    --algo hybrid \
    --key <key>

# Ví dụ AES S1:
python3 scripts/measure_metrics.py \
    --original  videos/output.h264 \
    --encrypted results/output.h264/aes_s1/output.h264.s1_aes \
    --decrypted results/output.h264/aes_s1/output.h264.s1_aes.decrypted \
    --strategy S1 --algo aes --key <key>

# Tùy chọn thêm:
#   --encrypted2 <file>   # file encrypted với key lệch 1 bit → đo NPCR/UACI key-sensitivity
#   --wrong-key  <key>    # test giải mã sai key
#   --timing-runs 3       # chạy 3 lần lấy trung bình
#   --nalu-info  <path>   # mặc định tự tìm nalu_cache/<video>.nalu_info.bin
#   --max-frames 60       # số frame tối đa để phân tích visual
```

**Lưu ý `--algo`**:
- `hybrid` → đo đủ tất cả metrics kể cả plaintext sensitivity (Wu et al. 2011)
- `aes` / `des` / `rc4` → bỏ qua plaintext sensitivity (không dùng `nalu_index`)

**Output**: file `metrics_report_<strategy>_<timestamp>.txt` trong thư mục hiện tại.

Chi tiết đầy đủ: xem [scripts/MEASURE_METRICS_GUIDE.md](scripts/MEASURE_METRICS_GUIDE.md)

---

## Build

```bash
make          # build tất cả binaries
make clean    # xoá binaries
```

Binaries được tạo:
- `pipelines/pipeline_hybrid_s1_h264analyze`
- `pipelines/pipeline_hybrid_s2_h264analyze`
- `pipelines/pipeline_hybrid_decrypt_s1_h264analyze`
- `pipelines/pipeline_hybrid_decrypt_s2_h264analyze`
- `extract/extract_nalu_from_h264analyze`

---

## IEncryption Interface

```cpp
class IEncryption {
public:
    virtual std::vector<uint8_t> encrypt(const std::vector<uint8_t>& dc_data,
                                          const std::vector<uint8_t>& key, int nalu_index) = 0;
    virtual std::vector<uint8_t> decrypt(const std::vector<uint8_t>& encrypted_data,
                                          const std::vector<uint8_t>& key, int nalu_index) = 0;
    static std::unique_ptr<IEncryption> create(const std::string& algo_name);
};
```

Thêm thuật toán mới: tạo `FooAlgo : public IEncryption`, đăng ký trong `encryption_factory.cpp`.

---

## Thiết kế thuật toán (mục đích nghiên cứu)

| Algo    | Keystream per NALU? | Điểm yếu |
|---------|---------------------|-----------|
| hybrid  | Có (PLCM + nalu_index) | — (đây là algo ta đề xuất) |
| aes     | Không (CTR counter=0) | Identical DC → identical ciphertext |
| des     | Không (OFB counter=0) | Identical DC → identical ciphertext |
| rc4     | Không (same key → same PRGA) | Keystream reuse |

AES/DES/RC4 cố ý dùng keystream cố định để thể hiện điểm yếu so với hybrid (keystream thay đổi theo nalu_index).

---

## Output pipeline

| File | Nội dung |
|------|---------|
| `results/<video>/<algo>_s<N>/<video>.<suffix>` | File H.264 đã mã hoá |
| `results/<video>/<algo>_s<N>/<video>.<suffix>.meta` | Strategy, NALU counts |
| `results/<video>/<algo>_s<N>/<video>.dc_dump.txt` | DC bytes trước encrypt |
| `results/<video>/<algo>_s<N>/<video>.<suffix>.decrypted` | Kết quả giải mã |
| `results/<video>/<algo>_s<N>/<video>.<suffix>.dc_dump_decrypted.txt` | DC bytes sau decrypt |

Suffixes: `.s1_hybrid_fixed`, `.s1_aes`, `.s1_des`, `.s1_rc4`, `.s2_*` tương tự.

---

## Xác minh (bắt buộc sau mỗi decrypt)

```bash
cmp --silent videos/output.h264 \
    results/output.h264/aes_s1/output.h264.s1_aes.decrypted \
    && echo 'MATCH' || echo 'DIFFER'
```

---

## Giá trị tham chiếu (canonical test — output.h264)

| Chỉ số | Giá trị |
|--------|---------|
| Tổng NALU (h264_analyze) | 19,348 |
| Type 1 (P/B frames) | 18,882 |
| Type 5 (I-frames) | 155 |
| S1 NALUs encrypted | 19,037 |
| S2 NALUs encrypted | 155 |

Tốc độ mã hoá S1 trên output.h264 (DC_ONLY_TIME_MS):
| Algo | ~ms | ns/NALU |
|------|-----|---------|
| rc4 | 10 | 549 |
| aes | 58 | 3,045 |
| hybrid | 555 | 29,153 |
| des | 477 | 25,044 |

---

## Vấn đề đã biết

**EPB (Emulation Prevention Byte) discrepancy**
- Naive detector thấy 19,349 NALU, h264_analyze thấy 19,348 — lệch 1
- Giải pháp: luôn dùng h264_analyze-based pipelines

**FFmpeg báo lỗi khi decode file đã mã hoá**
- Expected behavior — payload NALU không còn đúng H.264 syntax
- Giải pháp: decrypt trước, decode sau
