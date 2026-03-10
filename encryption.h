#pragma once

#include <cstdint>
#include <vector>

// DC/MV Encryption per Algorithm 1 (BOE - Block-level Observation Encryption)
// D'_ij = s^t ⊕ D_ij (XOR for DC)
// M'_ij = s^t ⊗ M_ij (modular arithmetic for MV)

class Encryption {
public:
    // Initialize encryption with key and block index
    Encryption(const std::vector<uint8_t>& key, int mb_index);
    
    // Encrypt DC coefficient: D' = keystream ⊕ D
    int encrypt_dc(int dc);
    
    // Decrypt DC coefficient: D = keystream ⊕ D'
    int decrypt_dc(int dc);
    
    // Encrypt MV component: M' = (keystream * M) mod 256
    int encrypt_mv(int mv);
    
    // Decrypt MV component: M = (M' * inv_keystream) mod 256
    int decrypt_mv(int mv_enc);
    
    // Get keystream for current block
    uint8_t get_keystream() const { return keystream_; }
    
private:
    std::vector<uint8_t> key_;
    int mb_index_;
    uint8_t keystream_;
    
    // Generate keystream from key and block index
    uint8_t generate_keystream(const std::vector<uint8_t>& key, int mb_index);
};

