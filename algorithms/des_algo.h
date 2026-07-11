#pragma once
#include "encryption_interface.h"

/**
 * DESAlgo — DES-CTR with fixed counter, pure C++ (no OpenSSL).
 *
 * Generates a 25-byte keystream via DES-CTR (4 independent counter blocks
 * 0..3 encrypted with DES-ECB) and XORs with the DC block. Counter is fixed
 * (nalu_index ignored), demonstrating keystream-reuse weakness.
 *
 * Key derivation: user-supplied bytes are XOR-folded into 8 bytes (not a
 * standard KDF — intentional simplification for research comparison).
 */
class DESAlgo : public IEncryption {
public:
    std::vector<uint8_t> encrypt(
        const std::vector<uint8_t>& dc_data,
        const std::vector<uint8_t>& key,
        int nalu_index
    ) override;

    std::vector<uint8_t> decrypt(
        const std::vector<uint8_t>& encrypted_data,
        const std::vector<uint8_t>& key,
        int nalu_index
    ) override;

    // Verifies the DES block cipher against the classic Stallings test vector.
    static bool self_test();
};
