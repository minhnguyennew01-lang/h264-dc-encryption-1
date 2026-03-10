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

