# Bug Report: DC_SIZE=25 nhưng DC thực tế chỉ có 24 bytes

**Mức độ**: Trung bình (không crash, không mất dữ liệu, nhưng encrypt 1 byte ngoài vùng DC)  
**Trạng thái**: Đang hoạt động bình thường nhờ cơ chế ghi đè đối xứng — nhưng không đúng thiết kế gốc

---

## Mô tả

Pipeline encrypt/decrypt đang đọc và ghi **25 bytes** bắt đầu từ `DC_OFFSET=19`, trong khi số DC coefficient thực tế trong một macroblock H.264 là **24 bytes** (16 luma DC + 4 Cb DC + 4 Cr DC).

Byte thứ 25 (ở offset 43) **không phải DC coefficient** — đây là byte tiếp theo trong bitstream sau vùng DC.

---

## Vị trí lỗi

| File | Dòng | Nội dung |
|------|------|---------|
| `pipelines/pipeline_hybrid_s1_h264analyze.cpp` | 88 | `const int DC_OFFSET = 19, DC_SIZE = 25;` |
| `pipelines/pipeline_hybrid_s2_h264analyze.cpp` | (tương tự) | `DC_SIZE = 25` |
| `pipelines/pipeline_hybrid_decrypt_s1_h264analyze.cpp` | 99 | `DC_SIZE = 25` |
| `pipelines/pipeline_hybrid_decrypt_s2_h264analyze.cpp` | (tương tự) | `DC_SIZE = 25` |
| `scripts/measure_metrics.py` | 226 | `DC_SIZE = 25` (docstring ghi "Đọc 24 DC bytes" nhưng dùng 25) |

---

## Thiết kế gốc vs thực tế

### Thiết kế gốc (`hybrid_encryption.h`)

```
Padding: 24→25 bytes for 5×5 matrix
encrypt_hybrid: Input 24 bytes → pad đến 25 (vì Arnold cần 5×5=25) → trả về 25 bytes
decrypt_hybrid: Input 25 bytes → giải mã → bỏ padding → trả về 24 bytes
```

Padding byte được tính: `padding_byte = 25 - original_size = 25 - 24 = 1`  
Giá trị padding: chính là số byte thiếu (`1`), không phải `0x00`.

### Thực tế trong pipeline

```cpp
// Pipeline đọc đúng 25 bytes:
vector<uint8_t> dc_data(
    file_data.begin() + nalu_payload_start + DC_OFFSET,
    file_data.begin() + nalu_payload_start + DC_OFFSET + DC_SIZE);  // DC_SIZE=25

// encrypt() được gọi với 25 bytes:
vector<uint8_t> encrypted_dc = algo->encrypt(dc_data, key_bytes, (int)i);
```

Kết quả: `original_size = 25` → `padding_byte = 25 - 25 = 0` → cơ chế padding **không bao giờ được kích hoạt**.

### Decrypt cũng truyền `padding_byte = 0` cứng

```cpp
// hybrid_algo.cpp:17-18
// padding_byte = 0 matches original pipeline behaviour (it was always 0 there too)
return HybridEncryption::decrypt_hybrid(encrypted_data, key, nalu_index, 0);
```

`original_size = 25 - 0 = 25` → trả về 25 bytes → ghi đè toàn bộ 25 bytes vào file.

---

## Tại sao BER=0 vẫn đạt được

Pipeline encrypt và decrypt **đối xứng hoàn toàn**: cả hai đều đọc và ghi đúng cùng 1 vùng 25 bytes. Byte thứ 25 (không phải DC) cũng được mã hoá rồi giải mã đúng về giá trị gốc. Do đó SHA256 PASS và BER=0.

Đây là lý do lỗi không gây crash hay sai kết quả trong kiểm thử — nhưng về mặt kỹ thuật vẫn là sai thiết kế.

---

## Hậu quả

1. **Byte ngoài vùng DC bị mã hoá**: 1 byte ở offset 43 (không phải DC coefficient) bị thay đổi trong file encrypted. Nếu byte này thuộc syntax element quan trọng, decoder sẽ báo lỗi khi giải mã file encrypted (đây là expected behavior của selective encryption, nhưng nguyên nhân một phần do lỗi này).

2. **Cơ chế padding trong `encrypt_hybrid` không bao giờ chạy**: Code padding 24→25 tồn tại nhưng dead code trong pipeline hiện tại.

3. **Mâu thuẫn tài liệu**: `hybrid_encryption.h` ghi "Input: 24-byte DC data", `measure_metrics.py` ghi "Đọc 24 DC bytes" nhưng cả hai thực tế dùng 25.

---

## Cách sửa đề xuất

### Phương án A — Sửa pipeline về đúng 24 bytes (theo thiết kế gốc)

```cpp
const int DC_OFFSET = 19, DC_SIZE = 24;  // 24 DC bytes thực tế
```

`encrypt_hybrid` sẽ tự pad 24→25 bên trong (`padding_byte=1`), trả về 25 bytes.  
Decrypt pipeline cần lưu lại `padding_byte` từ encrypt để truyền vào `decrypt_hybrid`.

**Lưu ý**: Cần cơ chế lưu `padding_byte` per-NALU (hoặc tính lại từ `original_size`). Hiện tại pipeline decrypt không lưu thông tin này.

### Phương án B — Chấp nhận DC_SIZE=25 và cập nhật tài liệu

Giữ nguyên `DC_SIZE=25`, vùng mã hoá = 24 DC bytes + 1 byte tiếp theo.  
Cập nhật comment và header cho nhất quán: "25 bytes (24 DC + 1 adjacent byte)".  
Đơn giản hơn, không cần thay đổi logic.

---

## Khuyến nghị

**Phương án B** phù hợp hơn cho nghiên cứu hiện tại vì:
- Không phá vỡ invariant BER=0 đang hoạt động
- Không cần refactor pipeline decrypt để xử lý `padding_byte`
- Kết quả metrics (NPCR, UACI, Entropy) vẫn valid vì tính trên toàn bộ 25 bytes nhất quán

Cần cập nhật: comment trong `hybrid_encryption.h`, docstring `read_dc_blocks` trong `measure_metrics.py`, và mô tả trong thesis (nêu rõ "vùng mã hoá là 25 bytes bắt đầu từ offset 19").
