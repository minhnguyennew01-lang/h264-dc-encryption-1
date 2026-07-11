/**
 * RC4 stream cipher — pure C++, no external dependencies.
 */
#include "rc4_algo.h"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// RC4 key-scheduling algorithm (KSA) + pseudo-random generation (PRGA)
// ---------------------------------------------------------------------------

static std::vector<uint8_t> rc4_process(
    const std::vector<uint8_t>& data,
    const std::vector<uint8_t>& key)
{
    // KSA
    uint8_t S[256];
    for (int i = 0; i < 256; i++) S[i] = (uint8_t)i;

    size_t key_len = key.empty() ? 1 : key.size();
    int j = 0;
    for (int i = 0; i < 256; i++) {
        j = (j + S[i] + key[i % key_len]) & 0xFF;
        uint8_t tmp = S[i]; S[i] = S[j]; S[j] = tmp;
    }

    // PRGA
    std::vector<uint8_t> out(data.size());
    int idx = 0, jj = 0;
    for (size_t k = 0; k < data.size(); k++) {
        idx = (idx + 1) & 0xFF;
        jj  = (jj + S[idx]) & 0xFF;
        uint8_t tmp = S[idx]; S[idx] = S[jj]; S[jj] = tmp;
        out[k] = data[k] ^ S[(S[idx] + S[jj]) & 0xFF];
    }
    return out;
}

// ---------------------------------------------------------------------------
// RC4Algo public methods
// ---------------------------------------------------------------------------

std::vector<uint8_t> RC4Algo::encrypt(
    const std::vector<uint8_t>& dc_data,
    const std::vector<uint8_t>& key,
    int /*nalu_index*/)
{
    if (dc_data.size() < 25)
        throw std::runtime_error("RC4Algo::encrypt: dc_data too short ("
            + std::to_string(dc_data.size()) + " < 25)");
    return rc4_process(dc_data, key);
}

std::vector<uint8_t> RC4Algo::decrypt(
    const std::vector<uint8_t>& encrypted_data,
    const std::vector<uint8_t>& key,
    int /*nalu_index*/)
{
    if (encrypted_data.size() < 25)
        throw std::runtime_error("RC4Algo::decrypt: encrypted_data too short ("
            + std::to_string(encrypted_data.size()) + " < 25)");
    return rc4_process(encrypted_data, key);
}

// ---------------------------------------------------------------------------
// Wikipedia RC4 test vector: Key="Key" (4B 65 79), PT="Plaintext" (9 bytes)
// Expected CT: BB F3 16 E8 D9 40 AF 0A D3
// https://en.wikipedia.org/wiki/RC4#Test_vectors
// ---------------------------------------------------------------------------
bool RC4Algo::self_test() {
    static const uint8_t key_bytes[]  = {0x4b, 0x65, 0x79};
    static const uint8_t plain[]      = {0x50,0x6c,0x61,0x69,0x6e,0x74,0x65,0x78,0x74};
    static const uint8_t expected[]   = {0xbb,0xf3,0x16,0xe8,0xd9,0x40,0xaf,0x0a,0xd3};
    std::vector<uint8_t> key_vec(key_bytes, key_bytes + 3);
    std::vector<uint8_t> plain_vec(plain, plain + 9);
    auto ct = rc4_process(plain_vec, key_vec);
    if (ct.size() != 9) return false;
    return memcmp(ct.data(), expected, 9) == 0;
}
