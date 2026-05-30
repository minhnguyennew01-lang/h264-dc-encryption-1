#pragma once

#include <cstdint>
#include <vector>
#include <string>

/**
 * Hybrid Encryption: PLCM + Arnold 2D Cat Map + Diffusion XOR
 * 
 * Process:
 * 1. PLCM keystream generation (pseudo-random bytes)
 * 2. Arnold 2D Cat Map transformation (chaos diffusion)
 * 3. XOR with PLCM keystream (confusion)
 * 
 * Parameters:
 * - PLCM mu: 0.37 (control parameter)
 * - Arnold P: 3, Q: 11 (5×5 matrix)
 * - Padding: 24→25 bytes for 5×5 matrix
 */

class HybridEncryption {
public:
    /**
     * Encrypt 24-byte DC data using hybrid method
     * Returns 25 bytes (24 encrypted + 1 padding byte)
     */
    static std::vector<uint8_t> encrypt_hybrid(
        const std::vector<uint8_t>& dc_data,
        const std::vector<uint8_t>& key,
        int nalu_index,
        uint8_t& padding_byte
    );

    /**
     * Decrypt 25-byte encrypted data using hybrid method
     * Returns 24 bytes original DC
     */
    static std::vector<uint8_t> decrypt_hybrid(
        const std::vector<uint8_t>& encrypted_data,
        const std::vector<uint8_t>& key,
        int nalu_index,
        uint8_t padding_byte
    );

private:
    // Step 1: PLCM keystream generation
    static std::vector<uint8_t> generate_plcm_keystream(
        const std::vector<uint8_t>& key,
        int nalu_index,
        size_t length
    );

    // Step 2: Arnold 2D transformation
    static void apply_arnold_2d_forward(std::vector<std::vector<uint8_t>>& matrix);
    static void apply_arnold_2d_inverse(std::vector<std::vector<uint8_t>>& matrix);

    // Step 3: Diffusion XOR
    static void diffusion_xor(
        std::vector<uint8_t>& data,
        const std::vector<uint8_t>& keystream
    );

    // Helper: Reshape 1D to 2D (5×5 matrix)
    static std::vector<std::vector<uint8_t>> reshape_to_5x5(
        const std::vector<uint8_t>& data
    );

    // Helper: Flatten 5×5 matrix back to 1D
    static std::vector<uint8_t> flatten_from_5x5(
        const std::vector<std::vector<uint8_t>>& matrix
    );

    // Arnold Cat Map parameters
    static constexpr int ARNOLD_P = 3;
    static constexpr int ARNOLD_Q = 11;
    static constexpr double PLCM_MU = 0.37;
    static constexpr int MATRIX_SIZE = 5;
};
