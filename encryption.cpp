#include "encryption.h"
#include <cstring>

using namespace std;

Encryption::Encryption(const vector<uint8_t>& key, int mb_index)
    : key_(key), mb_index_(mb_index) {
    keystream_ = generate_keystream(key, mb_index);
}

// Simple keystream generation: XOR of key bytes combined with mb_index
uint8_t Encryption::generate_keystream(const vector<uint8_t>& key, int mb_index) {
    uint8_t ks = 0;
    
    // XOR all key bytes
    for (size_t i = 0; i < key.size(); ++i) {
        ks ^= key[i];
    }
    
    // Mix in block index
    ks ^= (uint8_t)(mb_index & 0xFF);
    ks ^= (uint8_t)((mb_index >> 8) & 0xFF);
    
    return ks;
}

int Encryption::encrypt_dc(int dc) {
    // D' = s^t ⊕ D_ij (XOR operation)
    int encrypted = dc ^ (int)keystream_;
    // Clamp to signed byte range if needed
    if (encrypted > 127) encrypted = encrypted - 256;
    if (encrypted < -128) encrypted = encrypted + 256;
    return encrypted;
}

int Encryption::decrypt_dc(int dc) {
    // XOR is symmetric: D = s^t ⊕ D'
    return encrypt_dc(dc);
}

int Encryption::encrypt_mv(int mv) {
    // M' = s^t * M_ij mod 256 (modular multiplication)
    // Use simplified approach: multiply and mod 256
    int ks = (int)keystream_;
    int encrypted = (mv * ks) % 256;
    if (encrypted < 0) encrypted += 256;
    // Clamp to signed range
    if (encrypted > 127) encrypted = encrypted - 256;
    return encrypted;
}

int Encryption::decrypt_mv(int mv_enc) {
    // Find modular inverse of keystream mod 256 (if exists)
    // For simplicity: use table-based modular inverse
    // In production: implement extended Euclidean algorithm
    
    uint8_t ks = keystream_;
    
    // Find modular inverse using extended GCD
    // For now, use approximation or lookup table
    // Simple approach: if gcd(ks, 256) != 1, inverse may not exist
    // Use brute force for small numbers
    
    for (int i = 1; i < 256; ++i) {
        if (((ks * i) & 0xFF) == 1) {
            // Found modular inverse
            int decrypted = (mv_enc * i) % 256;
            if (decrypted > 127) decrypted -= 256;
            return decrypted;
        }
    }
    
    // If no inverse found, return encrypted value unchanged
    // (happens when gcd(ks, 256) != 1)
    return mv_enc;
}

// Utility function: Encrypt entire NALU by XORing specific bytes
// This is a simplified approach that modifies the NALU to make it different
std::vector<uint8_t> encrypt_dc_coefficients(
    const std::vector<uint8_t>& nalu,
    const std::vector<uint8_t>& key
) {
    // Make a copy
    std::vector<uint8_t> encrypted = nalu;
    
    // Skip NALU header (first 4 bytes: start code)
    // For each potential DC coefficient location, apply XOR with key
    
    if (encrypted.size() <= 4) {
        return encrypted;  // Too small to process
    }
    
    // Create multiple Encryption objects for different macroblocks
    // Apply encryption to bytes representing potential DC values
    // This is simplified: we XOR specific bytes with key-derived values
    
    for (size_t i = 4; i < encrypted.size(); i += 16) {
        // Create encryption for this block
        Encryption enc(key, i / 16);
        
        // Apply XOR transformation to 1-2 bytes per block
        // (simulating DC coefficient modification)
        if (i < encrypted.size()) {
            encrypted[i] ^= enc.get_keystream();
        }
        if (i + 1 < encrypted.size()) {
            encrypted[i + 1] ^= enc.get_keystream();
        }
    }
    
    return encrypted;
}

// Utility function: Decrypt entire NALU
std::vector<uint8_t> decrypt_dc_coefficients(
    const std::vector<uint8_t>& nalu,
    const std::vector<uint8_t>& key
) {
    // XOR is symmetric: decryption is same as encryption
    return encrypt_dc_coefficients(nalu, key);
}


