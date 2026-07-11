/**
 * DES ECB implementation — pure C++, no external dependencies.
 *
 * Reference: FIPS 46-3 / ANSI X3.92.
 */
#include "des_algo.h"
#include <cstring>

// ---------------------------------------------------------------------------
// DES permutation tables (1-indexed, as in the standard)
// ---------------------------------------------------------------------------

// Initial permutation
static const int IP[64] = {
    58,50,42,34,26,18,10, 2,
    60,52,44,36,28,20,12, 4,
    62,54,46,38,30,22,14, 6,
    64,56,48,40,32,24,16, 8,
    57,49,41,33,25,17, 9, 1,
    59,51,43,35,27,19,11, 3,
    61,53,45,37,29,21,13, 5,
    63,55,47,39,31,23,15, 7
};

// Final permutation (inverse of IP)
static const int FP[64] = {
    40, 8,48,16,56,24,64,32,
    39, 7,47,15,55,23,63,31,
    38, 6,46,14,54,22,62,30,
    37, 5,45,13,53,21,61,29,
    36, 4,44,12,52,20,60,28,
    35, 3,43,11,51,19,59,27,
    34, 2,42,10,50,18,58,26,
    33, 1,41, 9,49,17,57,25
};

// Expansion permutation (32→48 bits)
static const int E[48] = {
    32, 1, 2, 3, 4, 5,
     4, 5, 6, 7, 8, 9,
     8, 9,10,11,12,13,
    12,13,14,15,16,17,
    16,17,18,19,20,21,
    20,21,22,23,24,25,
    24,25,26,27,28,29,
    28,29,30,31,32, 1
};

// Permutation P (after S-boxes, 32 bits)
static const int P[32] = {
    16, 7,20,21,29,12,28,17,
     1,15,23,26, 5,18,31,10,
     2, 8,24,14,32,27, 3, 9,
    19,13,30, 6,22,11, 4,25
};

// Permuted Choice 1 (64→56 bits for key schedule)
static const int PC1[56] = {
    57,49,41,33,25,17, 9,
     1,58,50,42,34,26,18,
    10, 2,59,51,43,35,27,
    19,11, 3,60,52,44,36,
    63,55,47,39,31,23,15,
     7,62,54,46,38,30,22,
    14, 6,61,53,45,37,29,
    21,13, 5,28,20,12, 4
};

// Permuted Choice 2 (56→48 bits)
static const int PC2[48] = {
    14,17,11,24, 1, 5,
     3,28,15, 6,21,10,
    23,19,12, 4,26, 8,
    16, 7,27,20,13, 2,
    41,52,31,37,47,55,
    30,40,51,45,33,48,
    44,49,39,56,34,53,
    46,42,50,36,29,32
};

// Left shifts per round
static const int SHIFTS[16] = {
    1,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1
};

// S-boxes
static const uint8_t SBOXES[8][4][16] = {
    { // S1
        {14, 4,13, 1, 2,15,11, 8, 3,10, 6,12, 5, 9, 0, 7},
        { 0,15, 7, 4,14, 2,13, 1,10, 6,12,11, 9, 5, 3, 8},
        { 4, 1,14, 8,13, 6, 2,11,15,12, 9, 7, 3,10, 5, 0},
        {15,12, 8, 2, 4, 9, 1, 7, 5,11, 3,14,10, 0, 6,13}
    },
    { // S2
        {15, 1, 8,14, 6,11, 3, 4, 9, 7, 2,13,12, 0, 5,10},
        { 3,13, 4, 7,15, 2, 8,14,12, 0, 1,10, 6, 9,11, 5},
        { 0,14, 7,11,10, 4,13, 1, 5, 8,12, 6, 9, 3, 2,15},
        {13, 8,10, 1, 3,15, 4, 2,11, 6, 7,12, 0, 5,14, 9}
    },
    { // S3
        {10, 0, 9,14, 6, 3,15, 5, 1,13,12, 7,11, 4, 2, 8},
        {13, 7, 0, 9, 3, 4, 6,10, 2, 8, 5,14,12,11,15, 1},
        {13, 6, 4, 9, 8,15, 3, 0,11, 1, 2,12, 5,10,14, 7},
        { 1,10,13, 0, 6, 9, 8, 7, 4,15,14, 3,11, 5, 2,12}
    },
    { // S4
        { 7,13,14, 3, 0, 6, 9,10, 1, 2, 8, 5,11,12, 4,15},
        {13, 8,11, 5, 6,15, 0, 3, 4, 7, 2,12, 1,10,14, 9},
        {10, 6, 9, 0,12,11, 7,13,15, 1, 3,14, 5, 2, 8, 4},
        { 3,15, 0, 6,10, 1,13, 8, 9, 4, 5,11,12, 7, 2,14}
    },
    { // S5
        { 2,12, 4, 1, 7,10,11, 6, 8, 5, 3,15,13, 0,14, 9},
        {14,11, 2,12, 4, 7,13, 1, 5, 0,15,10, 3, 9, 8, 6},
        { 4, 2, 1,11,10,13, 7, 8,15, 9,12, 5, 6, 3, 0,14},
        {11, 8,12, 7, 1,14, 2,13, 6,15, 0, 9,10, 4, 5, 3}
    },
    { // S6
        {12, 1,10,15, 9, 2, 6, 8, 0,13, 3, 4,14, 7, 5,11},
        {10,15, 4, 2, 7,12, 9, 5, 6, 1,13,14, 0,11, 3, 8},
        { 9,14,15, 5, 2, 8,12, 3, 7, 0, 4,10, 1,13,11, 6},
        { 4, 3, 2,12, 9, 5,15,10,11,14, 1, 7, 6, 0, 8,13}
    },
    { // S7
        { 4,11, 2,14,15, 0, 8,13, 3,12, 9, 7, 5,10, 6, 1},
        {13, 0,11, 7, 4, 9, 1,10,14, 3, 5,12, 2,15, 8, 6},
        { 1, 4,11,13,12, 3, 7,14,10,15, 6, 8, 0, 5, 9, 2},
        { 6,11,13, 8, 1, 4,10, 7, 9, 5, 0,15,14, 2, 3,12}
    },
    { // S8
        {13, 2, 8, 4, 6,15,11, 1,10, 9, 3,14, 5, 0,12, 7},
        { 1,15,13, 8,10, 3, 7, 4,12, 5, 6,11, 0,14, 9, 2},
        { 7,11, 4, 1, 9,12,14, 2, 0, 6,10,13,15, 3, 5, 8},
        { 2, 1,14, 7, 4,10, 8,13,15,12, 9, 0, 3, 5, 6,11}
    }
};

// ---------------------------------------------------------------------------
// Bit-level helpers (bits numbered 1..64)
// ---------------------------------------------------------------------------
static inline int get_bit(const uint8_t* block, int bit) {
    bit--; // to 0-based
    return (block[bit/8] >> (7 - (bit%8))) & 1;
}

static inline void set_bit(uint8_t* block, int bit, int val) {
    bit--; // to 0-based
    if (val)
        block[bit/8] |= (uint8_t)(1 << (7 - (bit%8)));
    else
        block[bit/8] &= (uint8_t)~(1 << (7 - (bit%8)));
}

// Apply a permutation table; in/out are bit arrays (accessed with get_bit)
static void permute(const uint8_t* in, uint8_t* out, const int* table, int n) {
    memset(out, 0, (n+7)/8);
    for (int i = 0; i < n; i++) {
        set_bit(out, i+1, get_bit(in, table[i]));
    }
}

// Left rotate a bit-array of `len` bits by `n` positions
static void left_rotate(uint8_t* bits, int len, int n) {
    for (int k = 0; k < n; k++) {
        int first = get_bit(bits, 1);
        for (int i = 1; i < len; i++)
            set_bit(bits, i, get_bit(bits, i+1));
        set_bit(bits, len, first);
    }
}

// ---------------------------------------------------------------------------
// Key schedule: generate 16 subkeys of 48 bits each
// ---------------------------------------------------------------------------
static void des_key_schedule(const uint8_t key[8], uint8_t subkeys[16][6]) {
    uint8_t permuted_key[7] = {0}; // 56 bits
    permute(key, permuted_key, PC1, 56);

    // Split into C (bits 1-28) and D (bits 29-56)
    uint8_t C[4] = {0}, D[4] = {0};
    for (int i = 1; i <= 28; i++) set_bit(C, i, get_bit(permuted_key, i));
    for (int i = 1; i <= 28; i++) set_bit(D, i, get_bit(permuted_key, i+28));

    for (int round = 0; round < 16; round++) {
        left_rotate(C, 28, SHIFTS[round]);
        left_rotate(D, 28, SHIFTS[round]);

        // Combine C and D into 56-bit CD
        uint8_t CD[7] = {0};
        for (int i = 1; i <= 28; i++) set_bit(CD, i,    get_bit(C, i));
        for (int i = 1; i <= 28; i++) set_bit(CD, i+28, get_bit(D, i));

        permute(CD, subkeys[round], PC2, 48);
    }
}

// ---------------------------------------------------------------------------
// DES F function
// ---------------------------------------------------------------------------
static uint32_t des_f(uint32_t R, const uint8_t subkey[6]) {
    // Expand R (32 bits) to 48 bits
    uint8_t R_block[4];
    R_block[0] = (R >> 24) & 0xFF;
    R_block[1] = (R >> 16) & 0xFF;
    R_block[2] = (R >>  8) & 0xFF;
    R_block[3] =  R        & 0xFF;

    uint8_t expanded[6] = {0};
    permute(R_block, expanded, E, 48);

    // XOR with subkey
    for (int i = 0; i < 6; i++) expanded[i] ^= subkey[i];

    // S-box substitution: 48 bits → 32 bits
    uint8_t sbox_out[4] = {0};
    for (int s = 0; s < 8; s++) {
        // Extract 6 bits for this S-box (bit index in expanded: s*6+1 .. s*6+6)
        int base = s*6 + 1;
        int b0 = get_bit(expanded, base);
        int b1 = get_bit(expanded, base+1);
        int b2 = get_bit(expanded, base+2);
        int b3 = get_bit(expanded, base+3);
        int b4 = get_bit(expanded, base+4);
        int b5 = get_bit(expanded, base+5);

        int row = (b0 << 1) | b5;
        int col = (b1 << 3) | (b2 << 2) | (b3 << 1) | b4;
        uint8_t val = SBOXES[s][row][col];

        // Place 4-bit result at position s*4+1 in sbox_out
        int out_base = s*4 + 1;
        set_bit(sbox_out, out_base,   (val >> 3) & 1);
        set_bit(sbox_out, out_base+1, (val >> 2) & 1);
        set_bit(sbox_out, out_base+2, (val >> 1) & 1);
        set_bit(sbox_out, out_base+3,  val       & 1);
    }

    // Permutation P
    uint8_t p_out[4] = {0};
    permute(sbox_out, p_out, P, 32);

    return ((uint32_t)p_out[0] << 24) | ((uint32_t)p_out[1] << 16) |
           ((uint32_t)p_out[2] <<  8) |  (uint32_t)p_out[3];
}

// ---------------------------------------------------------------------------
// Single DES block encrypt / decrypt (8 bytes)
// ---------------------------------------------------------------------------
static void des_block(const uint8_t in[8], uint8_t out[8],
                      const uint8_t subkeys[16][6], bool decrypt) {
    // Initial permutation
    uint8_t ip_out[8] = {0};
    permute(in, ip_out, IP, 64);

    uint32_t L = ((uint32_t)ip_out[0]<<24)|((uint32_t)ip_out[1]<<16)|
                 ((uint32_t)ip_out[2]<< 8)| (uint32_t)ip_out[3];
    uint32_t R = ((uint32_t)ip_out[4]<<24)|((uint32_t)ip_out[5]<<16)|
                 ((uint32_t)ip_out[6]<< 8)| (uint32_t)ip_out[7];

    for (int i = 0; i < 16; i++) {
        int rnd = decrypt ? (15 - i) : i;
        uint32_t tmp = R;
        R = L ^ des_f(R, subkeys[rnd]);
        L = tmp;
    }

    // Swap: put R first, then L
    uint8_t pre_fp[8];
    pre_fp[0] = (R >> 24); pre_fp[1] = (R >> 16); pre_fp[2] = (R >> 8); pre_fp[3] = R;
    pre_fp[4] = (L >> 24); pre_fp[5] = (L >> 16); pre_fp[6] = (L >> 8); pre_fp[7] = L;

    permute(pre_fp, out, FP, 64);
}

// ---------------------------------------------------------------------------
// Key derivation: fold key bytes into 8 bytes
// ---------------------------------------------------------------------------
static void derive_des_key(const std::vector<uint8_t>& key, uint8_t des_key[8]) {
    memset(des_key, 0, 8);
    for (size_t i = 0; i < key.size(); i++) {
        des_key[i % 8] ^= key[i];
    }
}

// ---------------------------------------------------------------------------
// DESAlgo public methods
// ---------------------------------------------------------------------------

// Generate 32-byte DES-CTR keystream from counter blocks 0..3.
// Each block is encrypted independently (no feedback chain = CTR, not OFB).
// Counter is fixed (no nalu_index) — demonstrates keystream-reuse weakness vs. hybrid.
static void des_ctr_keystream(const uint8_t des_key[8], uint8_t ks[32]) {
    uint8_t subkeys[16][6];
    des_key_schedule(des_key, subkeys);
    for (int blk = 0; blk < 4; blk++) {
        uint8_t ctr[8] = {0, 0, 0, 0, 0, 0, 0, (uint8_t)blk};
        des_block(ctr, ks + blk * 8, subkeys, false);
    }
}

std::vector<uint8_t> DESAlgo::encrypt(
    const std::vector<uint8_t>& dc_data,
    const std::vector<uint8_t>& key,
    int /*nalu_index*/)
{
    if (dc_data.size() < 25)
        throw std::runtime_error("DESAlgo::encrypt: dc_data too short ("
            + std::to_string(dc_data.size()) + " < 25)");
    uint8_t des_key[8];
    derive_des_key(key, des_key);
    uint8_t ks[32];
    des_ctr_keystream(des_key, ks);

    std::vector<uint8_t> out(25);
    for (int i = 0; i < 25; i++)
        out[i] = dc_data[i] ^ ks[i];
    return out;
}

std::vector<uint8_t> DESAlgo::decrypt(
    const std::vector<uint8_t>& encrypted_data,
    const std::vector<uint8_t>& key,
    int /*nalu_index*/)
{
    if (encrypted_data.size() < 25)
        throw std::runtime_error("DESAlgo::decrypt: encrypted_data too short ("
            + std::to_string(encrypted_data.size()) + " < 25)");
    // XOR is its own inverse — same keystream as encrypt
    uint8_t des_key[8];
    derive_des_key(key, des_key);
    uint8_t ks[32];
    des_ctr_keystream(des_key, ks);

    std::vector<uint8_t> out(25);
    for (int i = 0; i < 25; i++)
        out[i] = encrypted_data[i] ^ ks[i];
    return out;
}

// ---------------------------------------------------------------------------
// Classic DES test vector (NIST FIPS 46-3 / NBS FIPS Pub 74 standard example;
// reproduced in Stallings "Cryptography and Network Security" Appendix C).
// Key:   13 34 57 79 9B BC DF F1
// Input: 01 23 45 67 89 AB CD EF
// Expected output: 85 E8 13 54 0F 0A B4 05
// ---------------------------------------------------------------------------
bool DESAlgo::self_test() {
    static const uint8_t key[8]  = {0x13,0x34,0x57,0x79, 0x9b,0xbc,0xdf,0xf1};
    static const uint8_t plain[8]= {0x01,0x23,0x45,0x67, 0x89,0xab,0xcd,0xef};
    static const uint8_t expected[8] = {0x85,0xe8,0x13,0x54, 0x0f,0x0a,0xb4,0x05};
    uint8_t subkeys[16][6];
    des_key_schedule(key, subkeys);
    uint8_t ct[8];
    des_block(plain, ct, subkeys, false);
    if (memcmp(ct, expected, 8) != 0) return false;
    // Also verify decrypt is the inverse
    uint8_t recovered[8];
    des_block(ct, recovered, subkeys, true);
    return memcmp(recovered, plain, 8) == 0;
}
