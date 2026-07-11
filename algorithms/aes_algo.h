#pragma once
#include "encryption_interface.h"

/**
 * AESAlgo — AES-128-CTR with fixed counter, pure C++ (no OpenSSL).
 *
 * Generates a 25-byte keystream via AES-CTR(counter=0, counter=1) and XORs
 * with the DC block. Counter is fixed (nalu_index ignored), so the same
 * plaintext always produces the same ciphertext — demonstrating keystream-reuse
 * weakness for comparison against the hybrid algorithm.
 *
 * Key derivation: user-supplied bytes are XOR-folded into 16 bytes (not a
 * standard KDF — intentional simplification for research comparison).
 */
class AESAlgo : public IEncryption {
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

    // Verifies the AES-128 block cipher against NIST FIPS-197 Appendix B.
    static bool self_test();
};
