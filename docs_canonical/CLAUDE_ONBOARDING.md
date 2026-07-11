# CLAUDE ONBOARDING - H.264 DC ENCRYPTION PROJECT

Mục tiêu: Tài liệu này giải thích toàn bộ dự án cho Claude AI — ý nghĩa, luồng hoạt động, các tệp quan trọng, cách chạy và kiểm thử. Tài liệu viết bằng tiếng Việt (kèm các lệnh bằng shell/English). Claude có thể dùng nội dung này để phân tích mã, tạo issue, viết tests, hoặc tự động hoá workflow.

**Tóm tắt nhanh**
- Dự án: mã hoá các hệ số DC (DC coefficients) trong các NALU Type 1 (P/B) và Type 5 (I) của một file H.264 theo nhiều chiến lược (S1, S2). S3 trước đây tồn tại nhưng đã bị loại khỏi canonical và được lưu trữ ở `s3_artifacts/` / `old/`.
- Phương pháp chính: trích xuất danh sách NALU bằng `h264_analyze` → tạo `nalu_info.bin` → dùng offsets chính xác để mã hoá/giải mã trực tiếp trên file nhị phân (không re-encode) → metadata đi kèm.
- Ngôn ngữ: C++ (g++), Bash, Python (tooling), dùng `h264bitstream/h264_analyze` như parser tham chiếu.

**Luồng làm việc (high-level)**
1. Chuẩn bị input: `output.h264` (hoặc file .h264 bất kỳ).
2. Chạy extractor: `./extract_nalu_from_h264analyze <input.h264>` → tạo `nalu_info.bin`.
3. Biên dịch pipelines: S1/S2 encryptors & decryptors (Makefile.arnold2d hoặc g++ commands).
4. Mã hoá: `./pipeline_hybrid_s1_h264analyze output.h264 <key>` hoặc S2 tương tự.
5. Giải mã: `./pipeline_hybrid_decrypt_s1_h264analyze output.h264.s1_hybrid_fixed <key> nalu_info.bin`.
6. Xác minh: `sha256sum` hoặc `cmp` giữa `output.h264` và decrypted output; dùng `h264_analyze` để kiểm tra NALU counts.

**Công nghệ & thư viện chính**
- **C++ (g++)**: core encryption/decryption code (`hybrid_encryption.cpp`, pipelines).
- **Bash**: scripts và Makefiles (`Makefile.arnold2d`).
- **Python**: conversion & metadata utilities (`scripts/*`).
- **h264bitstream / h264_analyze**: parser được dùng làm nguồn đúng để tìm offsets NALU; wrapper `extract_nalu_from_h264analyze` tạo `nalu_info.bin`.
- **Encryption primitives**: PLCM keystream generator, Arnold 2D permutation (5×5), XOR diffusion — được kết hợp trong `hybrid_encryption.*`.

**Định dạng dữ liệu chính**
- **`nalu_info.bin`**: file nhị phân chứa số lượng NALU và danh sách vị trí/offset NALU. (Pipeline đòi hỏi file này để biết vị trí DC bytes trong file nhị phân gốc.)
- **`.meta`**: metadata dạng text/binary đi kèm file mã hoá, chứa strategy, counts, và thông tin mapping.
- **`output.h264.s1_hybrid_fixed`**: ví dụ file đã mã hoá bằng S1.
- **`output.h264.s1_hybrid_fixed.decrypted`**: kết quả giải mã.

**Cấu trúc repository (tệp/folder chính và vai trò)**
- **`hybrid_encryption.cpp` / `hybrid_encryption.h`**: thuật toán mã hoá (PLCM, Arnold 2D, XOR) — chỉnh sửa logic mã hoá/giải mã.
- **`dc_metadata.cpp` / `dc_metadata.h`**: đọc/ghi metadata, xử lý `.meta`.
- **`extract_nalu_from_h264analyze`**: binary wrapper cho `h264_analyze` (sản sinh `nalu_info.bin`).
- **`extract_nalu_from_h264analyze.cpp`**: nguồn của wrapper.
- **`extract_nalu_from_h264analyze_wrapper.sh`**: helper chạy wrapper (tùy hệ thống).
- **`pipeline_hybrid_s1_h264analyze` / `.cpp`**: pipeline S1 (encrypt all P/B+I) sử dụng `nalu_info.bin` → tạo `.s1_hybrid_fixed` và `.meta`.
- **`pipeline_hybrid_s2_h264analyze` / `.cpp`**: pipeline S2 (encrypt I-frames only).
- **`pipeline_hybrid_decrypt_s1_h264analyze` / `.cpp`**: decryptor S1 (dùng `nalu_info.bin` để viết trực tiếp các byte giải mã vào file gốc offsets).
- **`pipeline_hybrid_decrypt_s2_h264analyze` / `.cpp`**: decryptor S2.
- **`Makefile` / `Makefile.arnold2d` / `Makefile.iframe`**: các target build nhanh; `Makefile.arnold2d` là canonical build for h264_analyze integration.
- **`scripts/`**:
  - `setup_h264_analyze.sh`: helper để cài/hạ `h264_analyze` (nếu cần build local copy of h264bitstream).
  - `convert_meta_text_to_fixed_bin.py` / `generate_binary_meta_from_nalu.py`: chuyển đổi giữa các định dạng metadata legacy và định dạng binary mới.
- **`docs_canonical/`**: tài liệu chính đã canonicalized (RUN_GUIDE, COMPLETE_GUIDE, etc.).
- **`output.h264`** và `output.h264.*` (encrypted/decrypted/meta): dataset / artifacts được dùng cho test.
- **`test_*` scripts & `verify_nalu*`**: scripts để test/verify pipeline và NALU counts.

**Các tệp cần chú ý khi sửa đổi / mở rộng**
- **Thay đổi logic mã hoá/đảo (core)**: `hybrid_encryption.cpp`.
- **Metadata format**: `dc_metadata.*` và `scripts/*`.
- **NALU extraction**: `extract_nalu_from_h264analyze.cpp`.
- **Pipelines**: `pipeline_hybrid_s1_h264analyze.cpp`, `pipeline_hybrid_s2_h264analyze.cpp`, `pipeline_hybrid_decrypt_*` tương ứng.

**Cách build (máy dev Linux, bash)**
- Build all canonical targets (nhanh):
```bash
cd /home/minh/Documents/NCKH20262/bitstream_impl_arnold2d
make -f Makefile.arnold2d clean
make -f Makefile.arnold2d all
```
- Hoặc build cụ thể (g++ examples):
```bash
# Build S1 encrypt + decrypt (h264_analyze-aware)
g++ -std=c++11 -O2 -o pipeline_hybrid_s1_h264analyze \
  pipeline_hybrid_s1_h264analyze.cpp hybrid_encryption.cpp dc_metadata.cpp

# Build S1 decrypt
g++ -std=c++11 -O2 -o pipeline_hybrid_decrypt_s1_h264analyze \
  pipeline_hybrid_decrypt_s1_h264analyze.cpp hybrid_encryption.cpp dc_metadata.cpp
```

**Cách chuẩn bị `h264_analyze` và `nalu_info.bin`**
1. Nếu chưa có `h264_analyze`, dùng `scripts/setup_h264_analyze.sh` để build/install local copy.
2. Chạy extractor:
```bash
cd /home/minh/Documents/NCKH20262/bitstream_impl_arnold2d
./extract_nalu_from_h264analyze output.h264
# Kết quả: nalu_info.bin created in cwd
```

**Các lệnh chạy chính (examples)**
- Encrypt S1 (all frames):
```bash
./pipeline_hybrid_s1_h264analyze output.h264 my_secret_key
# creates: output.h264.s1_hybrid_fixed + output.h264.s1_hybrid_fixed.meta
```

- Decrypt S1 (use authoritative nalu_info.bin):
```bash
./pipeline_hybrid_decrypt_s1_h264analyze output.h264.s1_hybrid_fixed my_secret_key nalu_info.bin
# creates: output.h264.s1_hybrid_fixed.decrypted
```

- Encrypt S2 (I-frames only) and decrypt similarly using S2 binaries.

- Convert legacy metadata (if needed):
```bash
python3 scripts/convert_meta_text_to_fixed_bin.py <legacy_meta.txt> > fixed_meta.bin
```

**Xác minh byte-exact (golden checks)**
- So sánh SHA256:
```bash
sha256sum output.h264
sha256sum output.h264.s1_hybrid_fixed.decrypted
# Expect identical hashes for successful decrypt (S1/S2 canonical)
```
- Hoặc dùng `cmp`:
```bash
cmp --silent output.h264 output.h264.s1_hybrid_fixed.decrypted && echo 'MATCH' || echo 'DIFFER'
```
- Kiểm tra NALU counts / types with `h264_analyze`:
```bash
/path/to/h264_analyze output.h264.s1_hybrid_fixed.decrypted | grep "nal_unit_type:" | sort | uniq -c
```

**Dữ liệu kiểm thử và artifacts**
- `output.h264`: canonical test input (≈228 MB in workspace snapshot).
- `output.h264.s1_hybrid_fixed`, `output.h264.s2_hybrid_fixed`: encrypted outputs.
- `*.meta`: metadata for encrypted artifacts.
- `s3_artifacts/`, `old/`: nơi lưu lịch sử S3 nếu cần phân tích (S3 bị loại khỏi canonical docs).

**Vấn đề đã gặp & lessons learned**
- Re-encoding / EPB (emulation prevention bytes) manipulation khi cố gắng re-encode NALU có thể gây khác biệt byte-exact. Giải pháp: luôn dùng offsets thô từ `nalu_info.bin` và ghi byte giải mã trực tiếp vào file gốc (no re-encode).
- S3 selection logic ban đầu chọn không đúng NALU (không VCL-aware) — đã fix; tuy nhiên S3 vẫn bị loại ra vì rủi ro phức tạp.

**Kịch bản debug nhanh**
- Nếu decryptor báo skipped indices hoặc file hashes khác nhau:
  - Kiểm tra `nalu_info.bin` có khớp input không (đã chạy extractor trên cùng file input chưa?).
  - Chạy `h264_analyze` trên input + decrypted để kiểm tra số lượng NALU.
  - Kiểm tra `*.meta` xem strategy và counts khớp.

**Các file test & scripts**
- `test_and_save_results.sh`, `test_arnold_strategies.sh`, `verify_nalu*`, `test_nalu_count.cpp` — dùng để chạy các pipelines tự động và lưu kết quả.
- Nếu thêm CI: chạy `make -f Makefile.arnold2d all` → `./extract_nalu_from_h264analyze output.h264` → encrypt → decrypt → verify hashes.

**Gợi ý cho Claude (tác vụ có thể làm tự động)**
- Review code risks: tìm chỗ xử lý offsets NALU, EPB handling, memory IO.
- Tự động hóa full test: tạo script chạy từ input → extractor → encrypt S1/S2 → decrypt → sha256 verify; produce report.
- Tạo linter-style checks: ensure `nalu_info.bin` is generated from same input file before decrypt.
- Generate issue templates cho regressions (EPB mismatches, failed NALU counts).

**Where to start reading code (recommended order)**
1. `extract_nalu_from_h264analyze.cpp` — hiểu cách `nalu_info.bin` được tạo.
2. `dc_metadata.*` — metadata formats.
3. `hybrid_encryption.cpp/hybrid_encryption.h` — core encrypt/decrypt primitives.
4. `pipeline_hybrid_s1_h264analyze.cpp`, `pipeline_hybrid_decrypt_s1_h264analyze.cpp` — glue logic; see how offsets used to write bytes.
5. `scripts/setup_h264_analyze.sh` and `scripts/*` — helper tooling.

**Next steps I recommend**
- (Optional) Move `CLAUDE_ONBOARDING.md` -> root `README.md` or link from canonical docs.
- Add a small CI job that runs the canonical verification on a small sample (to ensure future PRs do not break byte-exact invariants).

---

If you want, tôi có thể:
- tạo `docs_canonical/CLAUDE_ONBOARDING.md` (đã tạo) — nội dung này;
- hoặc tạo thêm ví dụ script `scripts/run_full_verify.sh` để Claude có thể trigger full pipeline tự động.

Cho tôi biết nếu bạn muốn tôi thêm `run_full_verify.sh` vào `scripts/` và commit/push nó lên remote sau khi bạn login `gh` (hoặc cho phép tôi push giúp bạn).