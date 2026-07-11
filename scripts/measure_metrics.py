#!/usr/bin/env python3
"""
measure_metrics.py — Đo tự động bộ metrics đánh giá hệ thống mã hoá DC H.264
Metrics: NPCR, UACI, Entropy, Pixel Correlation, PSNR, SSIM, TSSIM, Throughput, BER, Codec Compliance

Yêu cầu:
  pip install numpy scipy scikit-image opencv-python

Cách dùng:
  python3 scripts/measure_metrics.py \
      --original output.h264 \
      --encrypted output.h264.s1_hybrid_fixed \
      --decrypted output.h264.s1_hybrid_fixed.decrypted \
      --strategy S1 \
      --key mysecretkey

Để test wrong-key:
  python3 scripts/measure_metrics.py ... --wrong-key wrongkey
"""

import argparse
import hashlib
import math
import os
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

import struct

import numpy as np

try:
    import cv2
except ImportError:
    cv2 = None

try:
    from skimage.metrics import structural_similarity as ssim_func
except ImportError:
    ssim_func = None

try:
    from scipy.stats import pearsonr as _pearsonr
except ImportError:
    _pearsonr = None

# ─── Utilities ───────────────────────────────────────────────────────────────

def read_frames_opencv(path: str, max_frames: int = 60):
    """Đọc tối đa max_frames frame từ video bằng OpenCV.
    Nếu OpenCV không đọc được raw H.264, fallback sang FFmpeg subprocess.
    Nếu cả hai đều fail, in warning và return None (không crash).
    """
    frames = []

    

    # --- Fallback: FFmpeg pipe rawvideo gray ---
    try:
        # Lấy width/height từ ffprobe để biết kích thước frame
        probe_cmd = [
            'ffprobe', '-v', 'error',
            '-select_streams', 'v:0',
            '-show_entries', 'stream=width,height',
            '-of', 'csv=p=0',
            path
        ]
        probe_result = subprocess.run(
            probe_cmd, capture_output=True, text=True, timeout=30
        )
        if probe_result.returncode != 0 or not probe_result.stdout.strip():
            print(f"  [WARN] Không xác định được kích thước frame từ {path} — bỏ qua frame reading")
            return None

        wh = probe_result.stdout.strip().split(',')
        if len(wh) < 2:
            print(f"  [WARN] ffprobe output không hợp lệ: {probe_result.stdout.strip()}")
            return None
        width, height = int(wh[0]), int(wh[1])
        frame_size = width * height  # 1 byte per pixel (gray)

        ffmpeg_cmd = [
            'ffmpeg', '-v', 'error',
            '-i', path,
            '-f', 'rawvideo',
            '-pix_fmt', 'gray',
            '-vframes', str(max_frames),
            'pipe:1'
        ]
        ffmpeg_result = subprocess.run(
            ffmpeg_cmd, capture_output=True, timeout=120
        )
        if ffmpeg_result.returncode != 0:
            print(f"  [WARN] FFmpeg pipe fail cho {path}: {ffmpeg_result.stderr.decode(errors='replace')[:200]}")
            return None

        raw = ffmpeg_result.stdout
        n_frames = len(raw) // frame_size
        if n_frames == 0:
            print(f"  [WARN] FFmpeg không xuất frame nào từ {path}")
            return None

        for i in range(min(n_frames, max_frames)):
            chunk = raw[i * frame_size:(i + 1) * frame_size]
            arr = np.frombuffer(chunk, dtype=np.uint8).reshape(height, width).astype(np.float64)
            frames.append(arr)
        return frames if frames else None

    except Exception as e:
        print(f"  [WARN] FFmpeg fallback lỗi khi đọc {path}: {e}")
        return None


def check_deps():
    missing = []
    if cv2 is None:
        missing.append("opencv-python (pip install opencv-python)")
    if ssim_func is None:
        missing.append("scikit-image (pip install scikit-image)")
    try:
        import numpy  # noqa
    except ImportError:
        missing.append("numpy (pip install numpy)")
    if missing:
        print("[WARN] Thiếu thư viện — một số metrics sẽ bị bỏ qua:")
        for m in missing:
            print(f"       pip install {m.split('(')[0].strip()}")
        if cv2 is None:
            print("[WARN] opencv-python không có — frame-based metrics sẽ bị skip, nhưng BER/SHA256 vẫn chạy được.")
    return len(missing) == 0

# ─── Security Metrics ────────────────────────────────────────────────────────

def calc_npcr_uaci(frames_enc1, frames_enc2):
    """NPCR và UACI: so sánh hai bản mã hoá với key lệch 1 bit.

    Tham số:
        frames_enc1: danh sách frame encrypted với key gốc
        frames_enc2: danh sách frame encrypted với key lệch 1 bit
    Thiết kế: đo sự khác biệt giữa hai bản encrypted — KHÔNG phải original vs encrypted.
    """
    npcr_list, uaci_list = [], []
    for fe1, fe2 in zip(frames_enc1, frames_enc2):
        diff = (fe1 != fe2).astype(np.float64)
        npcr = diff.mean() * 100.0
        uaci = (np.abs(fe1 - fe2) / 255.0).mean() * 100.0
        npcr_list.append(npcr)
        uaci_list.append(uaci)
    return np.mean(npcr_list), np.mean(uaci_list)


def calc_entropy(frames_enc):
    """Information entropy trung bình của các frame encrypted.

    Docstring: đây là entropy pixel decoded (proxy), không phải bitstream entropy.
    Output sẽ ghi rõ '(pixel-domain proxy)'.
    """
    entropies = []
    for frame in frames_enc:
        pixels = frame.flatten().astype(np.uint8)
        hist, _ = np.histogram(pixels, bins=256, range=(0, 256))
        hist = hist[hist > 0].astype(np.float64)
        prob = hist / hist.sum()
        entropies.append(-np.sum(prob * np.log2(prob)))
    return np.mean(entropies)


def calc_correlation(frames_enc, n_samples: int = 5000):
    """Hệ số tương quan pixel liền kề (horizontal, vertical, diagonal)."""
    h_corrs, v_corrs, d_corrs = [], [], []
    for i, frame in enumerate(frames_enc):
        rng = np.random.default_rng(42 + i)  # RNG riêng per-frame
        h, w = frame.shape
        if h < 2 or w < 2:
            return np.nan, np.nan, np.nan
        idx_y = rng.integers(0, h - 1, n_samples)
        idx_x = rng.integers(0, w - 1, n_samples)
        p  = frame[idx_y, idx_x]
        ph = frame[idx_y, idx_x + 1]
        pv = frame[idx_y + 1, idx_x]
        pd = frame[idx_y + 1, idx_x + 1]
        h_corrs.append(np.corrcoef(p, ph)[0, 1])
        v_corrs.append(np.corrcoef(p, pv)[0, 1])
        d_corrs.append(np.corrcoef(p, pd)[0, 1])
    return np.mean(h_corrs), np.mean(v_corrs), np.mean(d_corrs)


def calc_keyspace(strategy: str = 'S1'):
    """Tính lý thuyết keyspace size của hệ thống mã hoá.

    PLCM: key string -> hash -> float64 = 2^53 effective bits
    Arnold: P=3, Q=11 (cố định), iterations=1 (mặc định) — keyspace thực tế ~2^58,
            dưới ngưỡng 2^128 vì Arnold params cố định.
    Tổng: in ước tính bits và so sánh với ngưỡng 2^128.
    """
    # PLCM: key được hash (SHA256) rồi map vào float64 mu
    # float64 có 53 bits mantissa => effective keyspace ~ 2^53
    plcm_bits = 53

    # Arnold: P=3, Q=11 (fixed params theo project), iterations=1 (mặc định)
    # Số hoán vị có thể: P * Q * iterations = 3 * 11 * 1 = 33 => ~log2(33) ~ 5 bits
    # Keyspace thực tế ~2^58 (plcm + arnold), dưới ngưỡng 2^128 vì Arnold params cố định
    arnold_p = 3
    arnold_q = 11
    arnold_iters = 1
    arnold_combinations = arnold_p * arnold_q * arnold_iters
    arnold_bits = math.log2(arnold_combinations) if arnold_combinations > 0 else 0

    total_bits = plcm_bits + arnold_bits

    THRESHOLD_BITS = 128
    result = {
        'plcm_bits': plcm_bits,
        'arnold_bits': arnold_bits,
        'total_bits': total_bits,
        'meets_threshold': total_bits >= THRESHOLD_BITS,
        'threshold_bits': THRESHOLD_BITS,
        'note': 'Arnold params cố định (P=3,Q=11,iters=1) — keyspace tổng ~2^58, dưới ngưỡng 2^128. Cần mở rộng key space nếu muốn đạt chuẩn.',
    }
    return result

DC_OFFSET = 19   # bytes từ nal_pos đến DC coefficients
DC_SIZE   = 25   # 25 bytes DC (khớp với C++ pipeline DC_SIZE=25)


def parse_nalu_info(nalu_info_path: str):
    """Parse nalu_info.bin → list of (start_pos, nal_pos, type)."""
    with open(nalu_info_path, 'rb') as f:
        raw = f.read()
    count = struct.unpack_from('<I', raw, 0)[0]
    entries = []
    for i in range(count):
        off = 4 + i * 9
        start_pos, nal_pos, ntype = struct.unpack_from('<IIB', raw, off)
        entries.append((start_pos, nal_pos, int(ntype)))
    return entries


def read_dc_blocks(file_path: str, nalu_entries, strategy: str):
    """Đọc 24 DC bytes từ mỗi VCL NALU trong file dùng offset từ nalu_info.

    S1: type 1 và 5; S2: type 5 only.
    Bỏ qua NALU nếu file quá ngắn tại offset đó.
    """
    target_types = {1, 5} if strategy.upper() == 'S1' else {5}
    with open(file_path, 'rb') as f:
        file_data = f.read()
    blocks = []
    for _, nal_pos, ntype in nalu_entries:
        if ntype not in target_types:
            continue
        start = nal_pos + DC_OFFSET
        end   = start + DC_SIZE
        if end > len(file_data):
            continue
        blocks.append(np.frombuffer(file_data[start:end], dtype=np.uint8).copy())
    return blocks  # list of np.ndarray shape (24,)


def read_dc_blocks_indexed(file_path: str, nalu_entries, strategy: str):
    """Như read_dc_blocks nhưng cũng trả về index trong nalu_entries cho mỗi block.

    Cần thiết cho plaintext sensitivity test: index được truyền vào encrypt_hybrid
    làm nalu_index, phải khớp chính xác với index mà C++ pipeline dùng.
    Returns: (blocks, indices) — indices[i] là vị trí trong nalu_entries.
    """
    target_types = {1, 5} if strategy.upper() == 'S1' else {5}
    with open(file_path, 'rb') as f:
        file_data = f.read()
    blocks, indices = [], []
    for entry_idx, (_, nal_pos, ntype) in enumerate(nalu_entries):
        if ntype not in target_types:
            continue
        start = nal_pos + DC_OFFSET
        end   = start + DC_SIZE
        if end > len(file_data):
            continue
        blocks.append(np.frombuffer(file_data[start:end], dtype=np.uint8).copy())
        indices.append(entry_idx)
    return blocks, indices


def calc_bitstream_npcr_uaci(blocks_a, blocks_b):
    """NPCR và UACI trên DC bytes (bitstream domain).

    Có thể so sánh:
      - orig vs enc  (đo mức thay đổi do encryption)
      - enc1 vs enc2 (đo key sensitivity)
    """
    a = np.concatenate(blocks_a).astype(np.float64)
    b = np.concatenate(blocks_b).astype(np.float64)
    npcr = np.mean(a != b) * 100.0
    uaci = np.mean(np.abs(a - b) / 255.0) * 100.0
    return npcr, uaci


def calc_bitstream_entropy(blocks_enc):
    """Shannon entropy (bits/byte) trên toàn bộ encrypted DC bytes."""
    all_bytes = np.concatenate(blocks_enc).astype(np.uint8)
    hist = np.bincount(all_bytes, minlength=256).astype(np.float64)
    prob = hist[hist > 0] / len(all_bytes)
    return float(-np.sum(prob * np.log2(prob)))


def calc_bitstream_correlation(blocks_enc):
    """Byte correlation: intra-NALU (adjacent bytes) và inter-NALU (same position).

    Trả về (corr_intra, corr_inter) — kỳ vọng ≈ 0 nếu encryption tốt.
    Trả về (nan, nan) nếu thiếu scipy hoặc dữ liệu không đủ.
    """
    if _pearsonr is None:
        return float('nan'), float('nan')
    if len(blocks_enc) < 2:
        return float('nan'), float('nan')

    # Intra-NALU: pairs of adjacent bytes trong cùng NALU
    x_intra = np.concatenate([b[:-1] for b in blocks_enc]).astype(np.float64)
    y_intra = np.concatenate([b[1:]  for b in blocks_enc]).astype(np.float64)
    corr_intra, _ = _pearsonr(x_intra, y_intra)

    # Inter-NALU: same byte position across consecutive NALUs
    mat = np.array(blocks_enc, dtype=np.float64)  # shape (N, 24)
    inter_list = []
    for k in range(mat.shape[1]):
        col = mat[:, k]
        if col.std() > 0:
            r, _ = _pearsonr(col[:-1], col[1:])
            inter_list.append(r)
    corr_inter = float(np.mean(inter_list)) if inter_list else float('nan')

    return float(corr_intra), float(corr_inter)


# ─── Plaintext Sensitivity (Standard NPCR/UACI per Wu et al. 2011) ───────────
# Python reimplementation của encrypt_hybrid từ hybrid_encryption.cpp
# Dùng để tính C1=Enc(P1,K) và C2=Enc(P2,K) trong Python, so sánh C1 vs C2.

def _precompute_arnold_fwd_perm():
    """Precompute Arnold 5×5 forward permutation.

    perm[dst_flat] = src_flat  →  new_data = old_data[perm]  (numpy indexing)
    Params: N=5, P=3, Q=11 (hardcoded per project config)
    """
    N, P, Q = 5, 3, 11
    perm = [0] * 25
    for y in range(N):
        for x in range(N):
            x_new = (x + P * y) % N
            y_new = (Q * x + (P * Q + 1) * y) % N
            perm[y_new * N + x_new] = y * N + x
    return np.array(perm, dtype=np.int32)

_ARNOLD_FWD_PERM = _precompute_arnold_fwd_perm()


def _plcm_params_batch(key_bytes_list, nalu_indices_arr):
    """Tính PLCM (p, x0) cho một mảng nalu_indices, vectorized.

    Tái hiện chính xác generate_plcm_keystream() trong hybrid_encryption.cpp.
    Returns: (p_vec, x0_vec) — shape (N,), float64.
    """
    n = len(key_bytes_list)
    base_cp = sum(key_bytes_list[i] / (256.0 * (i + 1)) for i in range(n))
    base_ic = sum(key_bytes_list[i] / (256.0 * (n - i)) for i in range(n))
    # fmod wrap (khớp C++ fix: tránh clamp triệt tiêu ảnh hưởng nalu_index)
    nalu_cp = (nalu_indices_arr & 0xFF).astype(np.float64) / 512.0
    nalu_ic = nalu_indices_arr.astype(np.float64) / 65536.0  # safe cho mọi video (max ~65535 NALUs)
    cp = 0.01 + np.abs(base_cp + nalu_cp) % 0.48
    ic = 0.01 + np.abs(base_ic + nalu_ic) % 0.98
    p = np.where(cp >= 0.5, 1.0 - cp, cp)  # PLCM constructor fold
    return p, ic


def _plcm_keystream_batch(p_vec, x_vec, length=25):
    """Sinh PLCM keystreams cho N parameter-sets song song (vectorized numpy).

    Tái hiện PLCM::single_iterate() và keystream generation của C++.
    Returns: shape (N, length) uint8.
    """
    x = x_vec.copy()
    # Pre-iterate 1000 lần (khớp C++ constructor)
    for _ in range(1000):
        x_eff = np.where(x > 0.5, 1.0 - x, x)
        x = np.where(x_eff < p_vec, x_eff / p_vec, (x_eff - p_vec) / (0.5 - p_vec))
    # Sinh keystream
    N = len(p_vec)
    ks = np.zeros((N, length), dtype=np.uint8)
    for j in range(length):
        x_eff = np.where(x > 0.5, 1.0 - x, x)
        x = np.where(x_eff < p_vec, x_eff / p_vec, (x_eff - p_vec) / (0.5 - p_vec))
        ks[:, j] = (x * 256.0).astype(np.int64) & 0xFF
    return ks


def _chained_diffusion_batch(d, ks, seed_scalar):
    """Chained feedback diffusion khớp với C++ diffusion_xor mới.

    C[0] = ks[0] ^ ((data[0]+ks[0])%256) ^ seed
    C[i] = ks[i] ^ ((data[i]+ks[i])%256) ^ C[i-1]

    d  : shape (N, 25) uint8 — bị ghi đè in-place và return
    ks : shape (N, 25) uint8 — keystream
    seed_scalar : int — XOR của tất cả key bytes
    """
    N = d.shape[0]
    prev = np.full(N, seed_scalar, dtype=np.int32)
    for j in range(25):
        ks_j = ks[:, j].astype(np.int32)
        t    = (d[:, j].astype(np.int32) + ks_j) % 256
        d[:, j] = (ks_j ^ t ^ prev).astype(np.uint8)
        prev = d[:, j].astype(np.int32)
    return d


def _encrypt_hybrid_pair_batch(dc_orig_blocks, key_bytes_list, nalu_indices, enc_rounds=5):
    """Encrypt P1 (gốc) và P2 (P1 với byte[0] XOR 1) cho tất cả NALUs, vectorized.

    Khớp chính xác với C++ encrypt_hybrid sau khi nâng cấp diffusion:
      mỗi round: Arnold forward → chained feedback diffusion (không phải XOR đơn).

    Dùng byte[0] (không phải byte[1]) vì với chained diffusion, thay đổi byte[0]
    lan ra toàn bộ C[1..24] qua chain → NPCR tối đa ~99.55%.
    Byte[0] là fixed point Arnold (không qua permutation) nhưng diffusion chaining
    đảm bảo ảnh hưởng lan ra tất cả bytes sau đó.

    Returns: (c1, c2) — mỗi cái shape (N, 25) uint8.
    """
    N = len(dc_orig_blocks)
    d1 = np.zeros((N, 25), dtype=np.uint8)
    d2 = np.zeros((N, 25), dtype=np.uint8)
    for i, block in enumerate(dc_orig_blocks):
        blen = len(block)
        pb = 25 - blen
        d1[i, :blen] = block;  d1[i, blen:] = pb
        d2[i, :blen] = block;  d2[i, blen:] = pb
    d2[:, 0] ^= 1  # P2: flip byte[0] — chain diffusion lan ra byte[1..24]

    # seed = XOR tất cả key bytes (khớp C++: for(auto b:key) seed ^= b)
    seed = 0
    for b in key_bytes_list:
        seed ^= b

    idx_arr = np.array(nalu_indices, dtype=np.int64)

    for round_i in range(enc_rounds):
        p_vec, ic_vec = _plcm_params_batch(key_bytes_list, idx_arr + round_i)
        ks = _plcm_keystream_batch(p_vec, ic_vec, length=25)  # (N, 25)
        # Arnold forward (permutation)
        d1 = d1[:, _ARNOLD_FWD_PERM].copy()
        d2 = d2[:, _ARNOLD_FWD_PERM].copy()
        # Chained feedback diffusion
        _chained_diffusion_batch(d1, ks, seed)
        _chained_diffusion_batch(d2, ks, seed)
    return d1, d2


def calc_dc_npcr_uaci_plaintext_sensitivity(dc_orig_blocks, nalu_indices, key_str, enc_rounds=5):
    """NPCR và UACI chuẩn theo Wu et al. 2011 — đo trên DC bitstream domain.

    So sánh C1=Enc(P1,K) vs C2=Enc(P2,K) trong đó P2=P1 với byte[0] XOR 0x01.
    Dùng byte[0] vì chained feedback diffusion lan ra C[1..24] từ C[0] → NPCR ~99.5%.
    (Byte[0] là fixed point Arnold — không qua permutation — nhưng diffusion chaining
    đảm bảo ảnh hưởng lan ra toàn bộ 24 bytes còn lại.)
    Đây là plaintext sensitivity test (avalanche effect) — KHÔNG phải orig vs enc.
    Ngưỡng lý tưởng: NPCR > 99.6094%, UACI ≈ 33.4635%.
    """
    if not dc_orig_blocks or not key_str:
        return None, None
    key_bytes = [ord(c) for c in key_str]
    c1, c2 = _encrypt_hybrid_pair_batch(dc_orig_blocks, key_bytes, nalu_indices, enc_rounds)
    npcr = float((c1 != c2).mean() * 100.0)
    uaci = float(np.abs(c1.astype(np.int32) - c2.astype(np.int32)).mean() / 255.0 * 100.0)
    return npcr, uaci


# ─── Visual Quality Metrics ──────────────────────────────────────────────────

def calc_psnr(frames_a, frames_b):
    """PSNR giữa hai danh sách frame.

    Trả về tuple: (mean_finite_psnr, n_inf, total)
    - mean_finite_psnr: trung bình PSNR của các frame không phải inf (None nếu không có)
    - n_inf: số frame có PSNR = inf (MSE = 0, tức identical)
    - total: tổng số frame so sánh
    """
    psnrs = []
    for fa, fb in zip(frames_a, frames_b):
        mse = np.mean((fa - fb) ** 2)
        if mse == 0:
            psnrs.append(float('inf'))
        else:
            psnrs.append(10 * math.log10(255.0 ** 2 / mse))

    total = len(psnrs)
    n_inf = sum(1 for v in psnrs if math.isinf(v))
    finite = [v for v in psnrs if not math.isinf(v)]
    mean_finite = np.mean(finite) if finite else None
    return mean_finite, n_inf, total


def calc_ssim(frames_a, frames_b):
    """SSIM trung bình."""
    if ssim_func is None:
        return None
    ssims = []
    for fa, fb in zip(frames_a, frames_b):
        s = ssim_func(fa.astype(np.uint8), fb.astype(np.uint8), data_range=255)
        ssims.append(s)
    return np.mean(ssims)


def calc_tssim(frames):
    """Temporal SSIM — SSIM giữa frame(t) và frame(t+1)."""
    if ssim_func is None or len(frames) < 2:
        return None
    tssims = []
    for i in range(len(frames) - 1):
        s = ssim_func(frames[i].astype(np.uint8), frames[i+1].astype(np.uint8), data_range=255)
        tssims.append(s)
    return np.mean(tssims)


def calc_dc_psnr(blocks_orig, blocks_enc):
    """PSNR trên DC domain: MSE giữa original DC bytes và encrypted DC bytes.

    Thấp (~8-10 dB) = encryption hiệu quả; inf = DC không thay đổi.
    """
    if not blocks_orig or not blocks_enc:
        return None
    a = np.concatenate(blocks_orig).astype(np.float64)
    b = np.concatenate(blocks_enc).astype(np.float64)
    mse = np.mean((a - b) ** 2)
    if mse == 0:
        return float('inf')
    return float(10 * math.log10(255.0 ** 2 / mse))


def calc_dc_ssim(blocks_orig, blocks_enc):
    """SSIM trên DC domain: tính per-NALU (25-byte window) rồi lấy trung bình.

    Dùng SSIM formula vectorized (Wang et al. 2004) với unbiased estimator ddof=1.
    Kết quả gần 0 hoặc âm = encryption phá vỡ cấu trúc DC.
    """
    if not blocks_orig or not blocks_enc:
        return None
    N_BYTES = 25
    A = np.array(blocks_orig, dtype=np.float64)  # (N, 24)
    B = np.array(blocks_enc,  dtype=np.float64)
    C1 = (0.01 * 255) ** 2
    C2 = (0.03 * 255) ** 2
    mu_a = A.mean(axis=1, keepdims=True)
    mu_b = B.mean(axis=1, keepdims=True)
    # ddof=1 (unbiased) per Wang et al. 2004
    var_a = A.var(axis=1, ddof=1, keepdims=True)
    var_b = B.var(axis=1, ddof=1, keepdims=True)
    cov   = ((A - mu_a) * (B - mu_b)).sum(axis=1, keepdims=True) / (N_BYTES - 1)
    ssim_per = ((2*mu_a*mu_b + C1) * (2*cov + C2)) / \
               ((mu_a**2 + mu_b**2 + C1) * (var_a + var_b + C2))
    return float(ssim_per.mean())


def calc_dc_tssim(blocks):
    """Temporal SSIM trên DC domain: SSIM giữa DC_block[i] và DC_block[i+1] (vectorized).

    Đo mức tương quan thời gian giữa các DC blocks liên tiếp trong danh sách.
    Gọi với dc_orig để lấy baseline, dc_enc để so sánh.
    Lưu ý: thực nghiệm cho thấy TSSIM DC encrypted ≥ TSSIM DC original — encryption
    KHÔNG làm giảm tương quan thời gian DC. DC-only selective encryption không phá vỡ
    temporal structure ở cấp bitstream thô.
    Với S2, các blocks là I-frames cách nhau ~GOP_size frames — không phải frame liền kề.
    """
    N_BYTES = 25
    if len(blocks) < 2:
        return None
    A = np.array(blocks[:-1], dtype=np.float64)  # (N-1, 24)
    B = np.array(blocks[1:],  dtype=np.float64)
    C1 = (0.01 * 255) ** 2
    C2 = (0.03 * 255) ** 2
    mu_a = A.mean(axis=1, keepdims=True)
    mu_b = B.mean(axis=1, keepdims=True)
    var_a = A.var(axis=1, ddof=1, keepdims=True)
    var_b = B.var(axis=1, ddof=1, keepdims=True)
    cov   = ((A - mu_a) * (B - mu_b)).sum(axis=1, keepdims=True) / (N_BYTES - 1)
    ssim_per = ((2*mu_a*mu_b + C1) * (2*cov + C2)) / \
               ((mu_a**2 + mu_b**2 + C1) * (var_a + var_b + C2))
    return float(ssim_per.mean())


# ─── Performance Metrics ─────────────────────────────────────────────────────

def measure_throughput(encrypt_cmd: list, input_file: str, runs: int = 3):
    """Đo throughput encrypt: trả về (MB/s, avg_sec, fps).

    FPS tính bằng cv2.VideoCapture (tổng frames / avg_sec).
    Nếu cv2 không có, tính xấp xỉ từ MB/s (giả định 2 MB/frame trung bình).
    """
    if not encrypt_cmd:
        return None, None, None
    file_mb = os.path.getsize(input_file) / (1024 * 1024)
    times = []
    for _ in range(runs):
        t0 = time.perf_counter()
        result = subprocess.run(encrypt_cmd, capture_output=True)
        t1 = time.perf_counter()
        if result.returncode == 0:
            times.append(t1 - t0)
    if not times:
        return None, None, None
    avg_sec = float(np.mean(times))
    mbps = file_mb / avg_sec

    # Tính FPS
    fps = None
    if cv2 is not None:
        try:
            cap = cv2.VideoCapture(input_file)
            total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
            cap.release()
            if total_frames > 0:
                fps = total_frames / avg_sec
        except Exception:
            pass
    return mbps, avg_sec, fps

# ─── Performance / Timing Metrics ────────────────────────────────────────────

def measure_pipeline_time(cmd: list, runs: int = 1):
    """Chạy pipeline command runs lần.

    Trả về (avg_sec, min_sec, max_sec, avg_dc_ms, avg_dc_ns_per_nalu).
    avg_dc_ms / avg_dc_ns_per_nalu là thời gian thuần encrypt/decrypt mảng DC
    (không tính I/O, NALU parsing), lấy từ marker ⏱  DC_ONLY_* in ra stdout.
    Trả về (None,None,None,None,None) nếu fail.
    """
    times = []
    dc_ms_list = []
    dc_ns_list = []
    for _ in range(runs):
        t0 = time.perf_counter()
        result = subprocess.run(cmd, capture_output=True)
        t1 = time.perf_counter()
        if result.returncode == 0:
            times.append(t1 - t0)
            stdout = result.stdout.decode(errors='replace')
            for line in stdout.splitlines():
                if 'DC_ONLY_TIME_MS:' in line:
                    try:
                        dc_ms_list.append(float(line.split('DC_ONLY_TIME_MS:')[-1].strip()))
                    except ValueError:
                        pass
                elif 'DC_ONLY_NS_PER_NALU:' in line:
                    try:
                        dc_ns_list.append(float(line.split('DC_ONLY_NS_PER_NALU:')[-1].strip()))
                    except ValueError:
                        pass
    if not times:
        return None, None, None, None, None
    avg_dc_ms = float(np.mean(dc_ms_list)) if dc_ms_list else None
    avg_dc_ns  = float(np.mean(dc_ns_list)) if dc_ns_list else None
    return float(np.mean(times)), float(np.min(times)), float(np.max(times)), avg_dc_ms, avg_dc_ns


def get_total_frames(video_path: str) -> int:
    """Đếm tổng số frame của video bằng cv2 hoặc ffprobe."""
    if cv2 is not None:
        try:
            cap = cv2.VideoCapture(video_path)
            n = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
            cap.release()
            if n > 0:
                return n
        except Exception:
            pass
    try:
        result = subprocess.run(
            ['ffprobe', '-v', 'error', '-select_streams', 'v:0',
             '-show_entries', 'stream=nb_frames', '-of', 'csv=p=0', video_path],
            capture_output=True, text=True, timeout=30
        )
        val = result.stdout.strip()
        if val and val.isdigit():
            return int(val)
    except Exception:
        pass
    return 0


# ─── Correctness Metrics ─────────────────────────────────────────────────────

def calc_ber(path_a: str, path_b: str):
    """Bit Error Rate giữa hai file nhị phân."""
    with open(path_a, 'rb') as fa, open(path_b, 'rb') as fb:
        data_a = fa.read()
        data_b = fb.read()
    if len(data_a) != len(data_b):
        delta = abs(len(data_a) - len(data_b))
        print(f"  [BER] Kích thước khác nhau: {len(data_a)} vs {len(data_b)} ({delta} bytes lệch)")
        return 1.0
    arr_a = np.frombuffer(data_a, dtype=np.uint8)
    arr_b = np.frombuffer(data_b, dtype=np.uint8)
    xor = np.bitwise_xor(arr_a, arr_b)
    bit_errors = np.unpackbits(xor).sum()
    total_bits = len(arr_a) * 8
    return bit_errors / total_bits


def sha256_file(path: str):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(65536), b''):
            h.update(chunk)
    return h.hexdigest()


def check_codec_compliance(path: str):
    """Thử decode bằng FFmpeg, trả về (ok: bool, output: str).

    ok = True chỉ khi returncode == 0 (không có lỗi decode).
    Bọc TimeoutExpired để tránh hang.
    """
    try:
        result = subprocess.run(
            ['ffmpeg', '-v', 'error', '-i', path, '-f', 'null', '-'],
            capture_output=True, text=True, timeout=120
        )
        errors = result.stderr.strip()
        ok = result.returncode == 0
        return ok, errors[:300] if errors else '(no errors)'
    except subprocess.TimeoutExpired:
        return False, '[TIMEOUT] ffmpeg decode vượt quá 120 giây'


def check_ffmpeg_available():
    try:
        subprocess.run(['ffmpeg', '-version'], capture_output=True, check=True)
        return True
    except (FileNotFoundError, subprocess.CalledProcessError):
        return False

# ─── Report ──────────────────────────────────────────────────────────────────

def print_section(title: str):
    print(f"\n{'='*60}")
    print(f"  {title}")
    print('='*60)


def fmt(val, decimals=4):
    if val is None:
        return "N/A (missing dep)"
    if isinstance(val, float) and math.isinf(val):
        return "inf (perfect)"
    if isinstance(val, float):
        return f"{val:.{decimals}f}"
    return str(val)


def fmt_psnr(mean_finite, n_inf, total):
    """Format kết quả calc_psnr() để in ra."""
    if mean_finite is None and n_inf == total:
        return f"inf (perfect) ({n_inf}/{total} frames perfect)"
    if mean_finite is None:
        return f"N/A (0 finite frames)"
    return f"{mean_finite:.2f} dB ({n_inf}/{total} frames perfect)"


def run_all(args):
    deps_ok = check_deps()
    if not deps_ok and cv2 is None:
        print("[WARN] opencv-python vắng mặt — frame-based metrics bị skip, BER/SHA256 vẫn tiếp tục.")

    report_lines = []

    def log(line=""):
        print(line)
        report_lines.append(line)

    ts = datetime.now().strftime('%Y%m%d_%H%M%S')
    algo = getattr(args, 'algo', 'hybrid')
    log(f"# Metrics Report — {args.strategy} / {algo.upper()}")
    log(f"Original : {args.original}")
    log(f"Encrypted: {args.encrypted}")
    log(f"Decrypted: {args.decrypted}")
    log(f"Algorithm: {algo}")
    log(f"Date     : {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")

    # ── Load frames ──
    print_section("Đang đọc frames...")
    frames_orig = frames_enc = frames_dec = None
    try:
        frames_orig = read_frames_opencv(args.original, args.max_frames)
        frames_enc  = read_frames_opencv(args.encrypted, args.max_frames)
        if args.decrypted and os.path.exists(args.decrypted):
            frames_dec = read_frames_opencv(args.decrypted, args.max_frames)
    except Exception as e:
        print(f"  [WARN] Lỗi khi đọc frames: {e}")

    if frames_orig:
        print(f"  Đọc được {len(frames_orig)} frames từ original")
    else:
        print("  [WARN] Không đọc được frames — bỏ qua visual metrics")

    n = min(len(frames_orig or []), len(frames_enc or []))
    if n > 0:
        frames_orig = frames_orig[:n]
        frames_enc  = frames_enc[:n]

    # ── Security Metrics ──
    print_section("1. SECURITY METRICS")

    # --- Bitstream-domain metrics (primary) ---
    nalu_info_path = args.nalu_info if hasattr(args, 'nalu_info') else 'nalu_info.bin'
    nalu_entries = []  # populated below; reused in timing section
    dc_orig = []       # populated below; reused in visual quality section
    dc_enc  = []
    bs_metrics_ok = False
    if os.path.exists(nalu_info_path) and os.path.exists(args.original) and os.path.exists(args.encrypted):
        try:
            nalu_entries = parse_nalu_info(nalu_info_path)
            dc_orig = read_dc_blocks(args.original,   nalu_entries, args.strategy)
            dc_enc  = read_dc_blocks(args.encrypted,  nalu_entries, args.strategy)
            log(f"  [Bitstream] NALUs đọc được: {len(dc_enc)} (strategy {args.strategy})")

            # NPCR/UACI: orig vs enc
            npcr_oe, uaci_oe = calc_bitstream_npcr_uaci(dc_orig, dc_enc)
            log(f"  NPCR (orig→enc) : {fmt(npcr_oe, 4)}%  (ngưỡng: > 99.6%)")
            log(f"  UACI (orig→enc) : {fmt(uaci_oe, 4)}%  (ngưỡng: ~33.46%)")

            # NPCR/UACI: enc1 vs enc2 (key sensitivity) — nếu có --encrypted2
            if args.encrypted2 and os.path.exists(args.encrypted2):
                dc_enc2 = read_dc_blocks(args.encrypted2, nalu_entries, args.strategy)
                npcr_ks, uaci_ks = calc_bitstream_npcr_uaci(dc_enc, dc_enc2)
                log(f"  NPCR (key sens) : {fmt(npcr_ks, 4)}%  (key sensitivity variant)")
                log(f"  UACI (key sens) : {fmt(uaci_ks, 4)}%  (key sensitivity variant)")
            else:
                log("  [SKIP] NPCR/UACI key-sensitivity: cần --encrypted2")

            # NPCR/UACI: plaintext sensitivity — chuẩn Wu et al. 2011
            # C1=Enc(P1,K) vs C2=Enc(P2,K), P2=P1 với byte[0] XOR 0x01
            # Chỉ áp dụng cho hybrid (Python reimplementation dùng nalu_index)
            if algo != 'hybrid':
                log(f"  [SKIP] Plaintext sensitivity: chỉ hỗ trợ hybrid (AES/DES/RC4 không dùng nalu_index)")
            elif args.key:
                log(f"  [Đang tính NPCR/UACI plaintext sensitivity — có thể mất vài giây...]")
                dc_orig_idx, nalu_idx_list = read_dc_blocks_indexed(
                    args.original, nalu_entries, args.strategy)
                npcr_ps, uaci_ps = calc_dc_npcr_uaci_plaintext_sensitivity(
                    dc_orig_idx, nalu_idx_list, args.key)
                if npcr_ps is not None:
                    log(f"  NPCR (plaintext sens): {fmt(npcr_ps, 4)}%  (chuẩn Wu 2011: > 99.6%)")
                    log(f"  UACI (plaintext sens): {fmt(uaci_ps, 4)}%  (chuẩn Wu 2011: ~33.46%)")
                else:
                    log("  [SKIP] Plaintext sensitivity: lỗi khi tính")
            else:
                log("  [SKIP] NPCR/UACI plaintext sensitivity: cần --key")

            # Entropy
            ent = calc_bitstream_entropy(dc_enc)
            log(f"  Entropy (DC bytes): {fmt(ent, 4)} bits/byte  (ngưỡng: > 7.9)")

            # Byte correlation
            c_intra, c_inter = calc_bitstream_correlation(dc_enc)
            if math.isnan(c_intra):
                log("  Byte Correlation : N/A (scipy thiếu hoặc dữ liệu không đủ)")
            else:
                log(f"  Byte Corr intra-NALU : {fmt(c_intra, 6)}  (ngưỡng: ≈ 0)")
                log(f"  Byte Corr inter-NALU : {fmt(c_inter, 6)}  (ngưỡng: ≈ 0)")

            bs_metrics_ok = True
        except Exception as e:
            log(f"  [ERROR] Bitstream metrics lỗi: {e}")
    else:
        log(f"  [SKIP] Bitstream metrics: không tìm thấy {nalu_info_path} hoặc file video")

    # --- Pixel-domain metrics (secondary, giữ để so sánh với literature) ---
    if frames_enc and n > 0:
        log(f"  --- Pixel-domain (secondary — error concealment proxy) ---")
        entropy_px = calc_entropy(frames_enc)
        log(f"  Entropy (pixel proxy): {fmt(entropy_px, 4)}")

        h_corr, v_corr, d_corr = calc_correlation(frames_enc)
        if isinstance(h_corr, float) and np.isnan(h_corr):
            log("  Pixel Correlation: N/A (uniform gray frame do error concealment)")
        else:
            log(f"  Pixel Corr H: {fmt(h_corr, 4)}")
            log(f"  Pixel Corr V: {fmt(v_corr, 4)}")
            log(f"  Pixel Corr D: {fmt(d_corr, 4)}")

    # Keyspace analysis (không đổi)
    if frames_enc and n > 0:
        ks = calc_keyspace(args.strategy)
        log(f"  Keyspace (PLCM):    ~2^{ks['plcm_bits']} bits")
        log(f"  Keyspace (Arnold):  ~2^{ks['arnold_bits']:.1f} bits (P={3}, Q={11}, iters=1)")
        log(f"  Keyspace (total):   ~2^{ks['total_bits']:.1f} bits")
        if not ks['meets_threshold']:
            log(f"  [WARNING] Keyspace < 2^{ks['threshold_bits']} — dưới ngưỡng khuyến nghị bảo mật!")
        else:
            log(f"  Keyspace >= 2^{ks['threshold_bits']} — OK")
    elif bs_metrics_ok:
        ks = calc_keyspace(args.strategy)
        log(f"  Keyspace (PLCM):    ~2^{ks['plcm_bits']} bits")
        log(f"  Keyspace (Arnold):  ~2^{ks['arnold_bits']:.1f} bits (P={3}, Q={11}, iters=1)")
        log(f"  Keyspace (total):   ~2^{ks['total_bits']:.1f} bits")
        if not ks['meets_threshold']:
            log(f"  [WARNING] Keyspace < 2^{ks['threshold_bits']} — dưới ngưỡng khuyến nghị bảo mật!")

    # ── Visual Quality Metrics ──
    print_section("2. VISUAL QUALITY METRICS (original vs encrypted)")

    if frames_orig and frames_enc and n > 0:
        mean_psnr_enc, n_inf_enc, total_enc = calc_psnr(frames_orig, frames_enc)
        log(f"  PSNR  : {fmt_psnr(mean_psnr_enc, n_inf_enc, total_enc)}  (mã hoá tốt nếu < 10 dB)")

        ssim_enc = calc_ssim(frames_orig, frames_enc)
        log(f"  SSIM  : {fmt(ssim_enc, 4)}   (mã hoá tốt nếu gần 0)")

        tssim_enc = calc_tssim(frames_enc)
        log(f"  TSSIM : {fmt(tssim_enc, 4)}   (temporal artifact — thấp = rõ hơn)")
    else:
        log("  [SKIP] Không có frames để tính visual metrics")

    log(f"  --- DC domain visual quality (bitstream domain) ---")
    if dc_orig and dc_enc:
        psnr_dc = calc_dc_psnr(dc_orig, dc_enc)
        if psnr_dc is None:
            log(f"  PSNR DC  : N/A")
        elif math.isinf(psnr_dc):
            log(f"  PSNR DC  : inf (DC bytes không thay đổi — mã hoá thất bại)")
        else:
            log(f"  PSNR DC  : {psnr_dc:.2f} dB  (thấp = encryption hiệu quả, ~8-10 dB lý tưởng)")

        ssim_dc = calc_dc_ssim(dc_orig, dc_enc)
        log(f"  SSIM DC  : {fmt(ssim_dc, 4)}  (gần 0/âm = mất cấu trúc DC; ∈[-1,1])")

        tssim_dc_orig = calc_dc_tssim(dc_orig)
        tssim_dc_enc  = calc_dc_tssim(dc_enc)
        s2_note = "  [*S2: giữa I-frames cách ~GOP_size frames, không phải frame liền kề]" \
                  if args.strategy.upper() == 'S2' else ""
        log(f"  TSSIM DC (original) : {fmt(tssim_dc_orig, 4)}  (baseline){s2_note}")
        log(f"  TSSIM DC (encrypted): {fmt(tssim_dc_enc,  4)}  (so với baseline — xem ghi chú)")
    else:
        log("  [SKIP] DC domain visual metrics: cần nalu_info.bin và files bitstream")

    # ── Correctness Metrics ──
    print_section("3. CORRECTNESS METRICS")

    if args.decrypted and os.path.exists(args.decrypted):
        sha_orig = sha256_file(args.original)
        sha_dec  = sha256_file(args.decrypted)
        match = sha_orig == sha_dec
        log(f"  SHA256 match  : {'PASS' if match else 'FAIL'}")
        log(f"    original : {sha_orig}")
        log(f"    decrypted: {sha_dec}")

        ber = calc_ber(args.original, args.decrypted)
        log(f"  BER           : {ber:.2e}  (phải = 0)")

        if frames_orig and frames_dec:
            nd = min(len(frames_orig), len(frames_dec))
            mean_psnr_dec, n_inf_dec, total_dec = calc_psnr(frames_orig[:nd], frames_dec[:nd])
            log(f"  PSNR (decrypt): {fmt_psnr(mean_psnr_dec, n_inf_dec, total_dec)}  (phải = inf)")
    else:
        log("  [SKIP] Không tìm thấy file decrypted")

    # ── Performance / Timing Metrics ──
    print_section("4. PERFORMANCE / TIMING METRICS")

    strategy_lower = args.strategy.lower()
    algo = getattr(args, 'algo', 'hybrid')
    enc_bin = f"./pipelines/pipeline_hybrid_{strategy_lower}_h264analyze"
    dec_bin = f"./pipelines/pipeline_hybrid_decrypt_{strategy_lower}_h264analyze"
    timing_runs = getattr(args, 'timing_runs', 1)

    if not args.key:
        log("  [SKIP] Timing: cần --key để chạy encrypt/decrypt pipeline")
    elif not os.path.exists(enc_bin):
        log(f"  [SKIP] Không tìm thấy binary: {enc_bin}")
    else:
        # Đếm tổng frames từ nalu_entries (Type 1 + Type 5); fallback sang cv2/ffprobe
        total_frames = 0
        if nalu_entries:
            total_frames = sum(1 for _, _, t in nalu_entries if t in (1, 5))
        if total_frames == 0:
            total_frames = get_total_frames(args.original)
        if total_frames > 0:
            log(f"  Video: {total_frames} frames tổng cộng")
        else:
            log("  [WARN] Không xác định được tổng số frames — speed (ms/frame) sẽ bị bỏ qua")

        # Encrypt timing
        enc_cmd = [enc_bin, args.original, args.key, "--algo", algo]
        avg_enc, min_enc, max_enc, dc_enc_ms, dc_enc_ns = measure_pipeline_time(enc_cmd, runs=timing_runs)
        if avg_enc is not None:
            log(f"  Encrypt time (avg/{timing_runs} run{'s' if timing_runs>1 else ''}): {avg_enc:.3f}s"
                + (f"  [min {min_enc:.3f}s / max {max_enc:.3f}s]" if timing_runs > 1 else ""))
            if total_frames > 0:
                log(f"  Encrypt speed       : {avg_enc / total_frames * 1000:.3f} ms/frame"
                    f"  ({total_frames / avg_enc:.1f} frames/s)")
            if dc_enc_ms is not None:
                log(f"  DC encrypt time     : {dc_enc_ms:.3f} ms total"
                    + (f"  ({dc_enc_ns:.0f} ns/NALU)" if dc_enc_ns is not None else ""))
        else:
            log(f"  [ERROR] Encrypt timing fail — binary trả lỗi")

        # Decrypt timing
        if not os.path.exists(dec_bin):
            log(f"  [SKIP] Decrypt timing: không tìm thấy {dec_bin}")
        elif not os.path.exists(args.encrypted):
            log(f"  [SKIP] Decrypt timing: không tìm thấy file encrypted")
        else:
            dec_cmd = [dec_bin, args.encrypted, args.key, "--algo", algo]
            avg_dec, min_dec, max_dec, dc_dec_ms, dc_dec_ns = measure_pipeline_time(dec_cmd, runs=timing_runs)
            if avg_dec is not None:
                log(f"  Decrypt time (avg/{timing_runs} run{'s' if timing_runs>1 else ''}): {avg_dec:.3f}s"
                    + (f"  [min {min_dec:.3f}s / max {max_dec:.3f}s]" if timing_runs > 1 else ""))
                if total_frames > 0:
                    log(f"  Decrypt speed       : {avg_dec / total_frames * 1000:.3f} ms/frame"
                        f"  ({total_frames / avg_dec:.1f} frames/s)")
                if dc_dec_ms is not None:
                    log(f"  DC decrypt time     : {dc_dec_ms:.3f} ms total"
                        + (f"  ({dc_dec_ns:.0f} ns/NALU)" if dc_dec_ns is not None else ""))
            else:
                log(f"  [ERROR] Decrypt timing fail — binary trả lỗi")

    print_section("5. CODEC COMPLIANCE TEST")

    if check_ffmpeg_available():
        log("  Kiểm tra file encrypted...")
        try:
            ok_enc, msg_enc = check_codec_compliance(args.encrypted)
            log(f"  Encrypted decode: {'OK' if ok_enc else 'ERRORS'}")
            log(f"    {'(stderr warnings): ' if ok_enc and msg_enc != '(no errors)' else ''}{msg_enc}")
        except Exception as e:
            log(f"  [ERROR] check_codec_compliance lỗi: {e}")

        if args.decrypted and os.path.exists(args.decrypted):
            log("  Kiểm tra file decrypted...")
            try:
                ok_dec, msg_dec = check_codec_compliance(args.decrypted)
                log(f"  Decrypted decode: {'OK (PASS)' if ok_dec else 'FAIL'}")
                log(f"    {'(stderr warnings): ' if ok_dec and msg_dec != '(no errors)' else ''}{msg_dec}")
            except Exception as e:
                log(f"  [ERROR] check_codec_compliance lỗi: {e}")
    else:
        log("  [SKIP] FFmpeg không tìm thấy — bỏ qua codec compliance")

    # ── Wrong Key Test ──
    if args.wrong_key and args.decrypted and os.path.exists(args.decrypted):
        print_section("6. WRONG-KEY TEST")
        wrong_dec = args.decrypted + ".wrongkey"
        strategy = args.strategy.lower()
        algo = getattr(args, 'algo', 'hybrid')
        decrypt_bin = f"./pipelines/pipeline_hybrid_decrypt_{strategy}_h264analyze"

        # Kiểm tra binary tồn tại trước khi chạy
        if not os.path.exists(decrypt_bin):
            log(f"  [SKIP] Binary không tồn tại: {decrypt_bin}")
        else:
            cmd = [decrypt_bin, args.encrypted, args.wrong_key, "--algo", algo]
            timeout_occurred = False
            try:
                result = subprocess.run(cmd, capture_output=True, timeout=300)
                if result.returncode != 0:
                    log(f"  [WARN] Decrypt wrong-key trả về returncode={result.returncode}")
            except subprocess.TimeoutExpired:
                log("  [ERROR] Wrong-key decrypt timeout sau 300 giây")
                timeout_occurred = True

            if not timeout_occurred:
                # Đổi tên output nếu tồn tại
                expected_out = args.encrypted + ".decrypted"
                if os.path.exists(expected_out) and expected_out != args.decrypted:
                    os.rename(expected_out, wrong_dec)

            if not timeout_occurred and os.path.exists(wrong_dec) and frames_orig:
                try:
                    frames_wrong = read_frames_opencv(wrong_dec, args.max_frames)
                    if frames_wrong:
                        nw = min(len(frames_orig), len(frames_wrong))
                        mean_psnr_w, n_inf_w, total_w = calc_psnr(frames_orig[:nw], frames_wrong[:nw])
                        log(f"  PSNR wrong-key: {fmt_psnr(mean_psnr_w, n_inf_w, total_w)}  (phải thấp = unviewable)")
                        ber_wrong = calc_ber(args.original, wrong_dec)
                        log(f"  BER wrong-key : {ber_wrong:.2e}  (phải > 0)")
                    else:
                        log("  [WARN] Không đọc được frames từ wrong-key output")
                except Exception as e:
                    log(f"  [ERROR] Wrong-key analysis lỗi: {e}")
            else:
                log("  [INFO] Wrong-key output không tìm thấy hoặc không có frames gốc để so sánh")

    # ── Save report ──
    print_section("DONE")
    report_path = f"metrics_report_{args.strategy.lower()}_{ts}.txt"
    with open(report_path, 'w') as f:
        f.write('\n'.join(report_lines))
    print(f"  Báo cáo đã lưu: {report_path}")


# ─── Main ────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Đo tự động bộ metrics đánh giá hệ thống mã hoá DC H.264"
    )
    parser.add_argument('--original',   required=True,  help='File H.264 gốc (e.g. videos/output.h264)')
    parser.add_argument('--encrypted',  required=True,  help='File H.264 đã mã hoá')
    parser.add_argument('--decrypted',  default=None,   help='File H.264 đã giải mã')
    parser.add_argument('--strategy',   default='S1',   choices=['S1', 'S2'], help='Chiến lược mã hoá')
    parser.add_argument('--algo',       default='hybrid', choices=['hybrid', 'aes', 'des', 'rc4'],
        help='Thuật toán mã hoá (default: hybrid)')
    parser.add_argument('--key',        default=None,   help='Key đã dùng để mã hoá')
    parser.add_argument('--wrong-key',  default=None,   dest='wrong_key', help='Key sai để test wrong-key scenario')
    parser.add_argument('--max-frames', default=60,     type=int, dest='max_frames', help='Số frame tối đa để phân tích (default: 60)')
    parser.add_argument('--encrypted2', default=None,
        help='File encrypted với key lệch 1 bit để đo NPCR/UACI key-sensitivity (tùy chọn)')
    parser.add_argument('--nalu-info', default=None, dest='nalu_info',
        help='File nalu_info.bin (default: tự derive từ nalu_cache/<video>.nalu_info.bin)')
    parser.add_argument('--timing-runs', default=1, type=int, dest='timing_runs',
        help='Số lần chạy để đo thời gian encrypt/decrypt (default: 1)')
    args = parser.parse_args()

    # Auto-derive nalu_info path nếu không truyền --nalu-info
    if not args.nalu_info:
        video_basename = Path(args.original).name
        args.nalu_info = f"nalu_cache/{video_basename}.nalu_info.bin"

    for path, label in [(args.original, '--original'), (args.encrypted, '--encrypted')]:
        if not os.path.exists(path):
            print(f"[ERROR] Không tìm thấy file {label}: {path}")
            sys.exit(1)

    if args.encrypted2 is not None and not os.path.exists(args.encrypted2):
        print(f"[ERROR] Không tìm thấy file --encrypted2: {args.encrypted2}")
        sys.exit(1)

    run_all(args)


if __name__ == '__main__':
    main()
