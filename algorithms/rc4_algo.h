#pragma once
#include "encryption_interface.h"

/**
 * RC4Algo — RC4 stream cipher, pure C++ (no OpenSSL).
 *
 * RC4 is a symmetric stream cipher: encrypt == decrypt (XOR with keystream).
 * Keystream depends only on the key (nalu_index ignored), so all NALUs share
 * the same keystream — demonstrating keystream-reuse weakness.
 * The key bytes are used directly (RC4 accepts variable-length keys).
 */
class RC4Algo : public IEncryption {
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

    // Verifies RC4 KSA+PRGA against the Wikipedia test vector (Key="Key").
    static bool self_test();
};
