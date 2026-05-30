#include "arnold_2d.h"
#include <cstring>
#include <iostream>

std::vector<std::vector<uint8_t>> Arnold2D::reshape_to_matrix(
    const std::vector<uint8_t>& data
) {
    std::vector<std::vector<uint8_t>> matrix(MATRIX_SIZE, 
                                            std::vector<uint8_t>(MATRIX_SIZE, 0));
    
    if (data.size() != TOTAL_BYTES) {
        std::cerr << "Error: Expected 25 bytes, got " << data.size() << "\n";
        return matrix;
    }
    
    for (int i = 0; i < MATRIX_SIZE; i++) {
        for (int j = 0; j < MATRIX_SIZE; j++) {
            matrix[i][j] = data[i * MATRIX_SIZE + j];
        }
    }
    
    return matrix;
}

std::vector<uint8_t> Arnold2D::flatten_matrix(
    const std::vector<std::vector<uint8_t>>& matrix
) {
    std::vector<uint8_t> data;
    data.reserve(TOTAL_BYTES);
    
    for (int i = 0; i < MATRIX_SIZE; i++) {
        for (int j = 0; j < MATRIX_SIZE; j++) {
            data.push_back(matrix[i][j]);
        }
    }
    
    return data;
}

void Arnold2D::apply_arnold_map(
    std::vector<std::vector<uint8_t>>& matrix
) {
    std::vector<std::vector<uint8_t>> temp = matrix;
    
    for (int y = 0; y < MATRIX_SIZE; y++) {
        for (int x = 0; x < MATRIX_SIZE; x++) {
            // Arnold Cat Map formula: (x', y') = ((x + P*y) mod N, (Q*x + (P*Q+1)*y) mod N)
            int x_new = (x + P * y) % MATRIX_SIZE;
            int y_new = (Q * x + (P * Q + 1) * y) % MATRIX_SIZE;
            
            matrix[y_new][x_new] = temp[y][x];
        }
    }
}

void Arnold2D::apply_inverse_arnold_map(
    std::vector<std::vector<uint8_t>>& matrix
) {
    std::vector<std::vector<uint8_t>> temp = matrix;
    
    // Inverse Arnold Cat Map
    // From: x' = (x + P*y) mod N, y' = (Q*x + (P*Q+1)*y) mod N
    // To:   x = (x' - P*y') mod N, y = (Q*x' - (P*Q+1)*y') mod N
    // Which simplifies to: x = ((P*Q+1)*x' - P*y') mod N, y = (-Q*x' + y') mod N
    
    for (int y_new = 0; y_new < MATRIX_SIZE; y_new++) {
        for (int x_new = 0; x_new < MATRIX_SIZE; x_new++) {
            // Inverse transformation
            int x = ((P * Q + 1) * x_new - P * y_new) % MATRIX_SIZE;
            int y = (-Q * x_new + y_new) % MATRIX_SIZE;
            
            // Handle negative modulo
            if (x < 0) x += MATRIX_SIZE;
            if (y < 0) y += MATRIX_SIZE;
            
            matrix[y][x] = temp[y_new][x_new];
        }
    }
}

std::vector<uint8_t> Arnold2D::encrypt_dc_arnold2d(
    const std::vector<uint8_t>& dc_data,
    uint8_t& padding_byte
) {
    if (dc_data.size() != DC_BYTES) {
        std::cerr << "Error: Expected " << DC_BYTES << " bytes DC, got " 
                  << dc_data.size() << "\n";
        return dc_data;
    }
    
    // Create 25-byte buffer with padding
    std::vector<uint8_t> data_with_padding(TOTAL_BYTES);
    
    // Copy DC data
    std::copy(dc_data.begin(), dc_data.end(), data_with_padding.begin());
    
    // Padding byte (can be 0x00 or random, here use 0x00 for simplicity)
    padding_byte = 0x00;
    data_with_padding[DC_BYTES] = padding_byte;
    
    // Reshape to 5×5 matrix
    auto matrix = reshape_to_matrix(data_with_padding);
    
    // Apply Arnold Cat Map transformation
    apply_arnold_map(matrix);
    
    // Flatten back to 1D
    auto encrypted_data = flatten_matrix(matrix);
    
    // Return only 24 bytes (remove padding from encrypted data)
    std::vector<uint8_t> result(encrypted_data.begin(), 
                               encrypted_data.begin() + DC_BYTES);
    
    return result;
}

std::vector<uint8_t> Arnold2D::decrypt_dc_arnold2d(
    const std::vector<uint8_t>& encrypted_dc,
    uint8_t padding_byte
) {
    if (encrypted_dc.size() != DC_BYTES) {
        std::cerr << "Error: Expected " << DC_BYTES << " bytes encrypted DC, got " 
                  << encrypted_dc.size() << "\n";
        return encrypted_dc;
    }
    
    // Reconstruct 25-byte buffer from 24 bytes + padding
    std::vector<uint8_t> data_with_padding(TOTAL_BYTES);
    
    // Copy encrypted 24 bytes
    std::copy(encrypted_dc.begin(), encrypted_dc.end(), data_with_padding.begin());
    
    // Add padding byte at the end
    data_with_padding[DC_BYTES] = padding_byte;
    
    // Reshape to 5×5 matrix
    auto matrix = reshape_to_matrix(data_with_padding);
    
    // Apply inverse Arnold Cat Map transformation
    apply_inverse_arnold_map(matrix);
    
    // Flatten back to 1D
    auto decrypted_data = flatten_matrix(matrix);
    
    // Return only 24 bytes (remove padding)
    std::vector<uint8_t> result(decrypted_data.begin(), 
                               decrypted_data.begin() + DC_BYTES);
    
    return result;
}
