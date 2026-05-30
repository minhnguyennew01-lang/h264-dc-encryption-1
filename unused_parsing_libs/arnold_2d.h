#pragma once

#include <cstdint>
#include <vector>

/**
 * Arnold 2D Cat Map - Square Matrix Permutation
 * 
 * DC size: 24 bytes (16Y + 4Cb + 4Cr)
 * Padded:  25 bytes (24 + 1 padding)
 * Matrix:  5×5 (25 elements)
 */

class Arnold2D {
public:
    // Parameters P and Q for Arnold Cat Map
    static constexpr int P = 3;
    static constexpr int Q = 11;
    static constexpr int MATRIX_SIZE = 5;  // 5×5 matrix
    static constexpr int TOTAL_BYTES = 25; // 24 + 1 padding
    static constexpr int DC_BYTES = 24;    // Original DC size
    
    /**
     * Encrypt DC with Arnold 2D Cat Map
     * @param dc_data: 24 bytes (16Y + 4Cb + 4Cr)
     * @param padding_byte: Output parameter - stores the padding byte (for metadata)
     * @return: 24 bytes encrypted (padding removed)
     */
    static std::vector<uint8_t> encrypt_dc_arnold2d(
        const std::vector<uint8_t>& dc_data,
        uint8_t& padding_byte
    );
    
    /**
     * Decrypt DC with Arnold 2D Cat Map
     * @param encrypted_dc: 24 bytes encrypted
     * @param padding_byte: The stored padding byte (from metadata)
     * @return: 24 bytes decrypted original
     */
    static std::vector<uint8_t> decrypt_dc_arnold2d(
        const std::vector<uint8_t>& encrypted_dc,
        uint8_t padding_byte
    );
    
private:
    /**
     * Reshape 1D vector to 2D matrix
     * @param data: 25 bytes
     * @return: 5×5 matrix (as 2D vector)
     */
    static std::vector<std::vector<uint8_t>> reshape_to_matrix(
        const std::vector<uint8_t>& data
    );
    
    /**
     * Flatten 2D matrix to 1D vector
     * @param matrix: 5×5 matrix
     * @return: 25 bytes
     */
    static std::vector<uint8_t> flatten_matrix(
        const std::vector<std::vector<uint8_t>>& matrix
    );
    
    /**
     * Apply Arnold Cat Map transformation
     * Formula: (x', y') = ((x + P*y) mod N, (Q*x + (P*Q+1)*y) mod N)
     */
    static void apply_arnold_map(
        std::vector<std::vector<uint8_t>>& matrix
    );
    
    /**
     * Apply Inverse Arnold Cat Map transformation
     */
    static void apply_inverse_arnold_map(
        std::vector<std::vector<uint8_t>>& matrix
    );
};
