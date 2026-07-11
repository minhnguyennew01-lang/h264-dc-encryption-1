#pragma once
#include "encryption_interface.h"
#include "hybrid_encryption.h"

/**
 * HybridAlgo — wraps HybridEncryption into the IEncryption interface.
 *
 * The underlying HybridEncryption::encrypt_hybrid() takes 24-byte dc_data
 * (offsets [0..23] of the 25-byte block) and produces 25 bytes.
 * HybridEncryption::decrypt_hybrid() takes those 25 bytes and returns 25
 * bytes where the first 24 are the recovered DC coefficients.
 *
 * Behaviour matches the original pipeline: padding_byte is discarded (always 0).
 */
class HybridAlgo : public IEncryption {
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
};
