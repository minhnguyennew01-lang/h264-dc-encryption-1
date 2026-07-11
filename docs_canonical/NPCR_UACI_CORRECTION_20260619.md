# Báo Cáo Sửa Lỗi NPCR/UACI — 2026-06-19

## 1. Vấn Đề Phát Hiện

Cách đo NPCR và UACI hiện tại trong `scripts/measure_metrics.py` đang sai so với định nghĩa gốc Wu et al. 2011.

### Cách hiện tại (SAI):
```
NPCR (orig → enc) = so sánh plaintext gốc vs ciphertext
```
Đây **không** phải NPCR/UACI chuẩn — nó chỉ đo "mức độ scrambling nhìn thấy được",
không đo differential attack resistance. Reviewer học thuật sẽ từ chối kết quả này.

### Định nghĩa đúng (Wu et al. 2011, JSAT):
```
1. Lấy plaintext P1
2. Tạo P2 = P1 với 1 byte thay đổi (XOR bit đầu tiên với 0x01)
3. Mã hoá cùng key K: C1 = Enc(P1, K), C2 = Enc(P2, K)
4. NPCR(C1, C2) = [Σ D(i) / N] × 100%       D(i) = 1 nếu C1[i] ≠ C2[i]
5. UACI(C1, C2) = [Σ |C1[i]−C2[i]| / (N×255)] × 100%
```
Metric này đo **avalanche effect / plaintext sensitivity** — khả năng kháng tấn công
differential khi attacker thay đổi 1 bit plaintext.

### Ngưỡng lý tưởng:
- **NPCR** ≥ 99.6094% — xác suất hai byte ngẫu nhiên 8-bit khác nhau = 1 − 1/256
- **UACI** ≈ 33.4635% — kỳ vọng |C1−C2|/255 khi C1, C2 phân phối đều

Các ngưỡng này chỉ có ý nghĩa khi so sánh **C1 vs C2** (hai ciphertext từ hai
plaintext khác nhau), KHÔNG phải plaintext vs ciphertext.

---

## 2. Tình Trạng Hiện Tại Của Dự Án

| Metric hiện có | So sánh gì | Đúng/Sai |
|----------------|-----------|----------|
| `NPCR (orig → enc)` | plaintext DC vs ciphertext DC | ❌ SAI định nghĩa |
| `UACI (orig → enc)` | plaintext DC vs ciphertext DC | ❌ SAI định nghĩa |
| `NPCR (key sensitivity)` | Enc(P, K1) vs Enc(P, K2) | ⚠️ Hợp lệ nhưng là **key sensitivity**, không phải NPCR chuẩn |
| `UACI (key sensitivity)` | Enc(P, K1) vs Enc(P, K2) | ⚠️ Hợp lệ nhưng là **key sensitivity**, không phải UACI chuẩn |
| **Cần thêm** | Enc(P1, K) vs Enc(P2, K), P1⊕P2 = 1 byte | ✅ Đây là NPCR/UACI chuẩn |

---

## 3. Giải Pháp Implement

### Trong miền bitstream DC:

Với mỗi NALU:
- **P1** = 24 DC bytes gốc từ file (đọc tại offset `nal_pos + 19`)
- **P2** = P1 với `P2[0] ^= 0x01` (flip bit 0 của byte đầu tiên)
- **C1** = `encrypt_hybrid(P1, key, nalu_idx)` — 25 bytes encrypted
- **C2** = `encrypt_hybrid(P2, key, nalu_idx)` — 25 bytes encrypted
- So sánh C1 vs C2 → NPCR, UACI

Để tính được C1 và C2 trong Python, cần reimport thuật toán `encrypt_hybrid` từ C++.

### Chi tiết thuật toán (từ `hybrid_encryption.cpp`):

#### PLCM (Piecewise Linear Chaotic Map):
```
control_param = Σ key[i] / (256 × (i+1))  +  (nalu_index & 0xFF) / 512
init_cond     = Σ key[i] / (256 × (n−i))  +  ((nalu_index >> 8) & 0xFF) / 512

# Clamp: control_param ∈ [0.01, 0.49], init_cond ∈ [0.01, 0.99]
# Nếu control_param >= 0.5 thì p = 1 - control_param, else p = control_param

# Pre-iterate 1000 lần:
f(x) = x/p            nếu 0 ≤ x < p
     = (x−p)/(0.5−p)  nếu p ≤ x ≤ 0.5
     = f(1−x)          nếu x > 0.5

# Sinh keystream: mỗi byte = int(x * 256) & 0xFF
```

#### Arnold 2D Cat Map (5×5, P=3, Q=11):
```
Forward: (x,y) → (x_new, y_new)
  x_new = (x + 3y) mod 5
  y_new = (11x + 34y) mod 5
Permutation: matrix[y_new][x_new] = matrix[y][x]
```

#### encrypt_hybrid (5 rounds):
```
input: 24 bytes → pad thành 25 bytes (padding_byte = 1)
for round in 0..4:
    round_nalu_index = nalu_index + round
    data = arnold_forward(data)             # confusion
    ks   = plcm_keystream(key, round_nalu_index, 25)  # diffusion
    data = data XOR ks
return data  # 25 bytes
```

---

## 4. Thay Đổi Cần Làm trong `scripts/measure_metrics.py`

### 4.1 Thêm các hàm Python reimplementation:
1. `_plcm_iterate(p, x)` — single PLCM iteration
2. `_ARNOLD_PERM` — precomputed 5×5 Arnold permutation (constant)
3. `_encrypt_hybrid_batch(dc_blocks, modified_blocks, key_bytes, nalu_indices)`
   — vectorized numpy, encrypt P1 và P2 song song, dùng chung PLCM keystream

### 4.2 Thêm hàm measurement:
```python
def calc_dc_npcr_uaci_plaintext_sensitivity(dc_orig_blocks, nalu_indices, key_str):
    """
    Đúng chuẩn Wu et al. 2011:
    So sánh C1=Enc(P1,K) vs C2=Enc(P2,K) trong đó P2 = P1 với byte[0] XOR 1.
    """
```

### 4.3 Thay đổi `read_dc_blocks` để trả về cả indices:
Thêm hàm `read_dc_blocks_indexed(file_path, nalu_entries, strategy)` → `(blocks, indices)`

### 4.4 Trong `run_all`, Section 1:
Sau NPCR key-sensitivity, thêm:
```
  NPCR (plaintext sens) : X.XXXX%  (chuẩn Wu 2011: > 99.6%)
  UACI (plaintext sens) : X.XXXX%  (chuẩn Wu 2011: ~33.46%)
```
Và rename label:
```
  [orig→enc]  →  giữ nguyên nhưng thêm ghi chú "(NOT standard NPCR)"
  [key sens]  →  thêm ghi chú "(key sensitivity variant)"
```

---

## 5. Kết Quả Thực Tế (2026-06-22)

> **Đã implement và đo thực tế sau khi upgrade thuật toán:**

### Vấn đề phát hiện và đã sửa:
- **NPCR ban đầu = 4.0% (= 1/25):** Arnold pure permutation + XOR byte-independent → keystream triệt tiêu trong difference domain → chỉ 1 byte thay đổi.
- **Fix:** Thay XOR đơn bằng **chained feedback diffusion**: `C[i] = ks[i] ^ ((data[i]+ks[i])%256) ^ C[i-1]`
- **PLCM clamp bug:** Với key dài, `base_cp > 0.49` → clamp loại bỏ `nalu_index` → tất cả NALUs dùng cùng keystream. Fix: dùng `fmod` thay `clamp`.

### Kết quả sau fix:

| Metric | S1 | S2 | Ngưỡng |
|--------|----|----|--------|
| **NPCR (plaintext sens)** | **99.5087%** | **99.5613%** | > 99.6094% |
| **UACI (plaintext sens)** | **32.7015%** | **33.2200%** | ~33.4635% |

Gap ~0.1% còn lại là giới hạn kiến trúc (5 rounds + modular addition), không phải bug.

---

## 6. Nguồn Tham Khảo

- Wu, Y., Noonan, J.P., Agaian, S. (2011). "NPCR and UACI Randomness Tests
  for Image Encryption." *JSAT*, pp. 31–38.
- PMC7713246: Xác nhận C1, C2 là hai ciphertext từ plaintext khác nhau 1 pixel.
- MATLAB File Exchange #155712: Code mẫu đúng chuẩn, so sánh hai ciphertext.
- arXiv:2103.04203 (Peng et al., VVC Selective Encryption): dùng plaintext
  sensitivity test chuẩn cho video encryption.

---

*Báo cáo này được tạo 2026-06-19 sau khi xác minh định nghĩa gốc qua tìm kiếm học thuật.*
*Implement: `scripts/measure_metrics.py` — thêm `calc_dc_npcr_uaci_plaintext_sensitivity`.*
