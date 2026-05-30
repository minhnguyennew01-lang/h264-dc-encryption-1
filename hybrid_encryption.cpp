#include "hybrid_encryption.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <cassert>

using namespace std;

// Multi-round encryption constant
static const int ENCRYPTION_ROUNDS = 1;

// ============================================================================
// PLCM Implementation (from user's reference algorithm)
// ============================================================================

class PLCM {
private:
    double p;
    
public:
    double x;
    
    PLCM(double control_param, double init_condition) {
        if (control_param >= 0.5) {
            control_param = 1.0 - control_param;
        }
        p = control_param;
        x = init_condition;
        
        // Pre-iterate 1000 times
        for (int i = 0; i < 1000; i++) {
            x = single_iterate(x);
        }
    }
    
    double single_iterate(double x_val) {
        if (0 <= x_val && x_val < p) {
            return x_val / p;
        } else if (p <= x_val && x_val <= 0.5) {
            return (x_val - p) / (0.5 - p);
        } else {
            // Recursive: x_val > 0.5
            return single_iterate(1.0 - x_val);
        }
    }
    
    vector<uint8_t> double_to_bytes(double value) {
        vector<uint8_t> result(6);
        for (int i = 0; i < 6; i++) {
            value *= 256.0;
            int byte_val = (int)value;
            result[i] = (uint8_t)(byte_val & 0xFF);
            value -= byte_val;
        }
        return result;
    }
};

// ============================================================================
// PLCM Keystream Generation
// ============================================================================

vector<uint8_t> HybridEncryption::generate_plcm_keystream(
    const vector<uint8_t>& key,
    int nalu_index,
    size_t length
) {
    vector<uint8_t> keystream(length);
    
    if (key.empty()) return keystream;
    
    // Derive PLCM parameters from key
    double control_param = 0.0;
    double init_condition = 0.0;
    
    for (size_t i = 0; i < key.size(); i++) {
        control_param += (double)key[i] / (256.0 * (i + 1));
        init_condition += (double)key[i] / (256.0 * (key.size() - i));
    }
    
    // Add NALU index influence
    control_param += (double)(nalu_index & 0xFF) / 512.0;
    init_condition += (double)((nalu_index >> 8) & 0xFF) / 512.0;
    
    // Clamp to valid ranges
    if (control_param < 0.01) control_param = 0.01;
    if (control_param > 0.49) control_param = 0.49;
    if (init_condition < 0.01) init_condition = 0.01;
    if (init_condition > 0.99) init_condition = 0.99;
    
    // Create PLCM with derived parameters
    PLCM plcm(control_param, init_condition);
    
    // Generate keystream
    for (size_t i = 0; i < length; i++) {
        plcm.x = plcm.single_iterate(plcm.x);
        keystream[i] = (uint8_t)(((int)(plcm.x * 256.0)) & 0xFF);
    }
    
    return keystream;
}

// ============================================================================
// Arnold 2D Cat Map: Forward Transform
// ============================================================================

void HybridEncryption::apply_arnold_2d_forward(vector<vector<uint8_t>>& matrix) {
    vector<vector<uint8_t>> temp = matrix;
    
    for (int y = 0; y < MATRIX_SIZE; y++) {
        for (int x = 0; x < MATRIX_SIZE; x++) {
            // Arnold Cat Map: (x', y') = ((x + P*y) mod N, (Q*x + (P*Q+1)*y) mod N)
            int x_new = (x + ARNOLD_P * y) % MATRIX_SIZE;
            int y_new = (ARNOLD_Q * x + (ARNOLD_P * ARNOLD_Q + 1) * y) % MATRIX_SIZE;
            
            matrix[y_new][x_new] = temp[y][x];
        }
    }
}

// ============================================================================
// Arnold 2D Cat Map: Inverse Transform
// ============================================================================

void HybridEncryption::apply_arnold_2d_inverse(vector<vector<uint8_t>>& matrix) {
    vector<vector<uint8_t>> temp = matrix;
    
    // Inverse: (x, y) = ((P*Q+1)*x - P*y, -Q*x + y) mod N
    for (int y = 0; y < MATRIX_SIZE; y++) {
        for (int x = 0; x < MATRIX_SIZE; x++) {
            int x_old = ((ARNOLD_P * ARNOLD_Q + 1) * x - ARNOLD_P * y) % MATRIX_SIZE;
            int y_old = (-ARNOLD_Q * x + y) % MATRIX_SIZE;
            
            // Handle negative modulo
            if (x_old < 0) x_old += MATRIX_SIZE;
            if (y_old < 0) y_old += MATRIX_SIZE;
            
            matrix[y_old][x_old] = temp[y][x];
        }
    }
}

// ============================================================================
// Diffusion XOR (apply XOR with PLCM keystream)
// ============================================================================

void HybridEncryption::diffusion_xor(
    vector<uint8_t>& data,
    const vector<uint8_t>& keystream
) {
    for (size_t i = 0; i < data.size() && i < keystream.size(); i++) {
        data[i] ^= keystream[i];
    }
}

// ============================================================================
// Reshape 1D → 5×5 Matrix
// ============================================================================

vector<vector<uint8_t>> HybridEncryption::reshape_to_5x5(
    const vector<uint8_t>& data
) {
    vector<vector<uint8_t>> matrix(MATRIX_SIZE, vector<uint8_t>(MATRIX_SIZE, 0));
    
    int idx = 0;
    for (int y = 0; y < MATRIX_SIZE && idx < (int)data.size(); y++) {
        for (int x = 0; x < MATRIX_SIZE && idx < (int)data.size(); x++) {
            matrix[y][x] = data[idx++];
        }
    }
    
    return matrix;
}

// ============================================================================
// Flatten 5×5 Matrix → 1D
// ============================================================================

vector<uint8_t> HybridEncryption::flatten_from_5x5(
    const vector<vector<uint8_t>>& matrix
) {
    vector<uint8_t> result;
    result.reserve(25);
    
    for (int y = 0; y < MATRIX_SIZE; y++) {
        for (int x = 0; x < MATRIX_SIZE; x++) {
            result.push_back(matrix[y][x]);
        }
    }
    
    return result;
}

// ============================================================================
// ENCRYPT: 3-Round Multi-Round Encryption
// Each round: Arnold 2D Forward (Confusion) → PLCM Keystream → XOR Diffusion
// Input: variable size (16, 4, or 24 bytes)
// Output: FULL 25 bytes (padded+encrypted)
// ============================================================================

vector<uint8_t> HybridEncryption::encrypt_hybrid(
    const vector<uint8_t>& dc_data,
    const vector<uint8_t>& key,
    int nalu_index,
    uint8_t& padding_byte
) {
    size_t original_size = dc_data.size();
    
    // Calculate padding amount
    padding_byte = 25 - original_size;
    
    // Pad to 25 bytes for Arnold 2D (5×5 matrix)
    // Use the padding_byte value itself as the padding bytes (not zeros!)
    vector<uint8_t> padded(25);
    memcpy(padded.data(), dc_data.data(), original_size);
    for (size_t i = original_size; i < 25; i++) {
        padded[i] = padding_byte;  // Fill padding with the padding amount itself
    }
    
    // Multi-round encryption (3 rounds)
    vector<uint8_t> data = padded;
    
    for (int round = 0; round < ENCRYPTION_ROUNDS; round++) {
        // Round-specific NALU index for keystream variation
        int round_nalu_index = nalu_index + round;
        
        // Step 1: Apply Arnold 2D Cat Map transformation (5×5 matrix) - Confusion
        vector<vector<uint8_t>> matrix = reshape_to_5x5(data);
        apply_arnold_2d_forward(matrix);
        vector<uint8_t> after_arnold = flatten_from_5x5(matrix);
        
        // Step 2: Generate PLCM keystream with round-adjusted index
        vector<uint8_t> plcm_keystream = generate_plcm_keystream(key, round_nalu_index, 25);
        
        // Step 3: Diffusion XOR with PLCM keystream
        data = after_arnold;
        diffusion_xor(data, plcm_keystream);
    }
    
    // Return FULL 25 bytes encrypted (DO NOT TRUNCATE!)
    return data;
}

// ============================================================================
// DECRYPT: 3-Round Multi-Round Encryption (REVERSE ORDER)
// Each round: XOR Diffusion (reverse) → Arnold 2D Inverse → Previous round output
// Rounds applied in reverse (2, 1, 0) to match encryption
// Input: 25 bytes encrypted
// Output: original_size bytes decrypted
// ============================================================================

vector<uint8_t> HybridEncryption::decrypt_hybrid(
    const vector<uint8_t>& encrypted_data,
    const vector<uint8_t>& key,
    int nalu_index,
    uint8_t padding_byte
) {
    // encrypted_data must be 25 bytes
    if (encrypted_data.size() != 25) {
        cerr << "❌ Error: decrypt_hybrid expects 25 bytes, got " << encrypted_data.size() << "\n";
        // Return zeros of same size as input
        return vector<uint8_t>(encrypted_data.size(), 0);
    }
    
    // Multi-round decryption in REVERSE order (rounds 2, 1, 0)
    vector<uint8_t> data = encrypted_data;
    
    for (int round = ENCRYPTION_ROUNDS - 1; round >= 0; round--) {
        // Round-specific NALU index for keystream variation (same as encryption)
        int round_nalu_index = nalu_index + round;
        
        // Step 1: Generate SAME PLCM keystream as encryption
        vector<uint8_t> plcm_keystream = generate_plcm_keystream(key, round_nalu_index, 25);
        
        // Step 2: Reverse diffusion XOR (XOR is its own inverse)
        vector<uint8_t> after_xor = data;
        diffusion_xor(after_xor, plcm_keystream);
        
        // Step 3: Apply Arnold 2D inverse transformation
        vector<vector<uint8_t>> matrix = reshape_to_5x5(after_xor);
        apply_arnold_2d_inverse(matrix);
        data = flatten_from_5x5(matrix);
    }
    
    // Remove padding and return original size
    size_t original_size = 25 - padding_byte;
    vector<uint8_t> result(data.begin(), data.begin() + original_size);
    return result;
}

