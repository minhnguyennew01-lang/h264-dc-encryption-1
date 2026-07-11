# Code Review — H.264 DC Selective Encryption
**Ngày:** 2026-06-16  
**Phạm vi:** Core thuật toán và pipeline encrypt/decrypt  
**Mục đích:** Nghiên cứu (NCKH 2026) — không deploy production  
**Rating tổng thể:** 5/10 (đủ dùng cho nghiên cứu, cần cải thiện nếu production)

---

## Các File Được Review

| File | Vai trò |
|------|---------|
| `hybrid_encryption.cpp` / `.h` | Core PLCM + Arnold 2D Cat Map + XOR |
| `pipeline_hybrid_s1_h264analyze.cpp` | Pipeline encrypt S1 |
| `pipeline_hybrid_s2_h264analyze.cpp` | Pipeline encrypt S2 |
| `pipeline_hybrid_decrypt_s1_h264analyze.cpp` | Pipeline decrypt S1 |
| `pipeline_hybrid_decrypt_s2_h264analyze.cpp` | Pipeline decrypt S2 |
| `extract_nalu_from_h264analyze.cpp` | Tạo `nalu_info.bin` |
| `dc_metadata.cpp` / `.h` | Metadata handling |

---

## Điểm Mạnh

- **Arnold inverse toán học đúng** — Forward matrix `[[1,3],[1,4]]` mod 5, det=1; inverse tính adjugate chính xác; scatter/gather direction nhất quán.
- **Byte-exact decrypt invariant giữ đúng** trong điều kiện bình thường — SHA256 PASS, BER=0 cho cả S1 và S2.
- **Offset nhất quán** — encrypt và decrypt đều dùng `nal_pos + 19`, 25 bytes, cùng padding convention.
- **Structure tương đối rõ** — phân tách encrypt core / pipeline glue / metadata rõ ràng.

---

## Vấn Đề Phát Hiện

### Critical (1)

#### PLCM Fixed-Point Trap
**File:** `hybrid_encryption.cpp:36–44`

Khi key tạo ra `init_condition = 0.5` chính xác (VD: key byte `0x80` → `128/256 = 0.5`):

```
single_iterate(0.5) → (0.5-p)/(0.5-p) = 1.0
single_iterate(1.0) → branch 3 → recurse(0.0)
single_iterate(0.0) → 0.0/p = 0.0  ← fixed point vĩnh viễn
```

Kết quả: toàn bộ keystream = `0x00` → XOR với 0 là identity → **DC bytes không bị mã hoá nhưng pipeline vẫn báo thành công**.

Clamp hiện tại (`< 0.01` hoặc `> 0.99`) không chặn được `x = 0.5`.

**Ảnh hưởng với key thông thường (`testkey`, `testkey2`):** Không trigger — đây là edge case với key đặc biệt.  
**Fix đề xuất (nếu cần):** Thêm guard sau derivation: `if (fabs(init_condition - 0.5) < 1e-10) init_condition += 0.001;`

---

### High (3)

#### 1. Command Injection (RCE Vector)
**File:** `extract_nalu_from_h264analyze.cpp:37`

```cpp
string cmd = "... timeout 300 " + h264_bin + " " + input_file + " 2>&1";
FILE* pipe = popen(cmd.c_str(), "r");
```

`input_file = argv[1]` không qua sanitization. Shell metacharacters (`;`, `$()`, backtick) trong filename → RCE.  
**Ảnh hưởng với dự án nghiên cứu:** Thấp — tool chỉ chạy locally với input file do researcher kiểm soát.

#### 2. `nalu_count` Không Được Validate
**File:** `pipeline_hybrid_s1_h264analyze.cpp:38` (và tương tự ở 3 file khác)

```cpp
uint32_t nalu_count = 0;
nalu_info.read((char*)&nalu_count, sizeof(nalu_count));
vector<NALUInfo> nalus(nalu_count);  // không validate, không check read() return
```

Nếu `nalu_info.bin` bị truncate (disk full khi extract): entries còn lại = zeros → `nal_pos = 0` → pipeline patch byte 19 của file header liên tục → silent corruption không có error message.  
**Ảnh hưởng với dự án:** Thấp nếu workflow chạy đúng thứ tự và disk có đủ không gian.

#### 3. S2 Per-NALU Key Diversification Yếu
**File:** `hybrid_encryption.cpp:76–83`

```cpp
control_param  += (double)(nalu_index & 0xFF) / 512.0;
init_condition += (double)((nalu_index >> 8) & 0xFF) / 512.0;
```

S2 encrypt 155 Type-5 NALUs với indices 0–154 (tất cả < 256). `(nalu_index >> 8) = 0` với mọi index < 256 → `init_condition` contribution = 0 cho **tất cả 155 NALUs của S2** → mọi NALU chia sẻ cùng init_condition, chỉ khác nhau qua `control_param` (8-bit).

**Ảnh hưởng:** Known-plaintext attack trên I-frame DC patterns khả thi hơn. Cần ghi nhận là limitation trong paper.

---

### Medium (3)

#### 1. `padding_byte` — Dead Out-Parameter (Bom Hẹn Giờ)
**File:** `pipeline_hybrid_s1_h264analyze.cpp:104`, `pipeline_hybrid_decrypt_s1_h264analyze.cpp:121`

`encrypt_hybrid` tính `padding_byte = 25 - dc_data.size()` nhưng caller không bao giờ đọc giá trị này. Decrypt hardcode `padding_byte = 0`.

Hiện tại hoạt động đúng vì `dc_data.size() = 25` → `padding_byte = 0`. Nếu ai sửa input sang 24 bytes (theo header comment sai), `padding_byte = 1` trong encrypt nhưng decrypt vẫn dùng `0` → ghi 1 byte garbage per NALU → byte-exact invariant vỡ silently.

Header comment tại `hybrid_encryption.h:24` cũng sai: *"Encrypt 24-byte DC data, Returns 25 bytes (24 encrypted + 1 padding)"* — thực tế là 25-byte input, 25 byte output, 0 byte padding được thêm vào file.

#### 2. Key In Ra stdout
**File:** `pipeline_hybrid_s1_h264analyze.cpp:60` (và 3 file còn lại)

```cpp
cout << "Key: " << key_str << "\n";
```

Key cleartext xuất hiện trong terminal history, CI logs, process listings. Với dự án nghiên cứu local: chấp nhận được. Với deploy thực tế: cần xóa hoặc log masked.

#### 3. Write `nalu_info.bin` Không Check Error
**File:** `extract_nalu_from_h264analyze.cpp:113–125`

`ofstream` mở file, write, close không check fail state. Disk full mid-write → file truncated → pipeline đọc sai positions. Tool in `"Extracted N NALUs"` không có error.

---

### Minor (3)

#### 1. `PLCM::double_to_bytes()` — Dead Code
**File:** `hybrid_encryption.cpp:47–56`

Method không có call site nào. Tạo ấn tượng sai rằng keystream extraction dùng 6 bytes/iteration (thực tế: 1 byte/iteration). Nếu maintainer "fix" bằng cách dùng method này → keystream 6x dài → mismatch encrypt/decrypt.

#### 2. `ENCRYPTION_ROUNDS = 1` Với Comment Sai
**File:** `hybrid_encryption.cpp`

Comment nói "3-Round Multi-Round" nhưng constant = 1. Artifact từ refactoring cũ chưa dọn.

#### 3. `NALUInfo` Struct Khai Báo Riêng Trong 5 File
Không có shared header. `size_t` vs `uint32_t` không nhất quán giữa các file. NALU loading loop copy-paste nguyên xi trong 4 files. Maintenance hazard khi cần thay đổi format.

---

## Tổng Kết

### Bảng Tóm Tắt

| Mức | Số | Vấn đề |
|-----|----|--------|
| Critical | 1 | PLCM fixed-point → keystream = 0 với key đặc biệt |
| High | 3 | Command injection, nalu_count unvalidated, S2 weak diversification |
| Medium | 3 | padding_byte trap, key logged, write not checked |
| Minor | 3 | Dead code, wrong comment, struct duplication |

### Đánh Giá Cho Mục Đích Nghiên Cứu

**Đủ dùng.** Các invariant cốt lõi (byte-exact decrypt, Arnold inverse đúng, offset nhất quán) hoạt động đúng với key thông thường và workflow chuẩn. Critical bug (PLCM fixed-point) chỉ trigger với key đặc biệt không gặp trong thực nghiệm. High bugs chỉ ảnh hưởng khi môi trường chạy không kiểm soát được (RCE qua filename, disk full).

**Cần ghi nhận trong paper:**
1. S2 per-NALU diversification yếu (High #3) — nêu là limitation của key schedule
2. Keyspace ~2^58 < 2^128 (đã biết) — nêu là hướng cải thiện
3. PLCM avalanche effect yếu (~30% key sensitivity) — đã báo cáo trong METRICS_REPORT_FINAL

**Nếu chuyển sang production trong tương lai**, ưu tiên fix theo thứ tự: Critical → High #1 (command injection) → Medium #1 (padding_byte interface) → High #2 → High #3.

---

*Review bởi reviewer agent — 2026-06-16*  
*Kết quả metrics đầy đủ: [METRICS_REPORT_FINAL_20260616.md](METRICS_REPORT_FINAL_20260616.md)*
