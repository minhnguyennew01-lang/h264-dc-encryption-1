#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

/**
 * IEncryption — common interface for all DC encryption algorithms.
 *
 * Contract:
 *   - encrypt() receives dc_data of exactly 25 bytes and returns 25 bytes.
 *   - decrypt() receives 25 bytes and returns 25 bytes.
 *   - Any internal padding/unpadding is hidden inside the implementation.
 */
class IEncryption {
public:
    virtual ~IEncryption() = default;

    // Encrypt 25 bytes of DC data; returns 25 bytes of ciphertext.
    virtual std::vector<uint8_t> encrypt(
        const std::vector<uint8_t>& dc_data,
        const std::vector<uint8_t>& key,
        int nalu_index
    ) = 0;

    // Decrypt 25 bytes of ciphertext; returns 25 bytes of plaintext.
    virtual std::vector<uint8_t> decrypt(
        const std::vector<uint8_t>& encrypted_data,
        const std::vector<uint8_t>& key,
        int nalu_index
    ) = 0;

    // Factory: create an encryptor by name ("hybrid", "aes", "des", "rc4").
    static std::unique_ptr<IEncryption> create(const std::string& algo_name);
};
