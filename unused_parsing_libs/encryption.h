#pragma once

#include <cstdint>
#include <vector>
#include <string>

// Keystream generator for PLCM-based encryption
class KeystreamGenerator {
public:
    void generateKeystream(const std::vector<uint8_t>& key, 
                          size_t length,
                          std::vector<uint8_t>& keystream);
};

class Encryption {
public:
    Encryption(const std::vector<uint8_t>& key, int mb_index);
    int encrypt_dc(int dc);
    int decrypt_dc(int dc);
    int encrypt_mv(int mv);
    int decrypt_mv(int mv_enc);
    uint8_t get_keystream() const { return keystream_; }
    
private:
    std::vector<uint8_t> key_;
    int mb_index_;
    uint8_t keystream_;
    uint8_t generate_keystream(const std::vector<uint8_t>& key, int mb_index);
};

std::vector<uint8_t> encrypt_dc_coefficients(
    const std::vector<uint8_t>& nalu,
    const std::vector<uint8_t>& key,
    int nalu_index
);

std::vector<uint8_t> decrypt_dc_coefficients(
    const std::vector<uint8_t>& nalu,
    const std::vector<uint8_t>& key,
    int nalu_index
);

// Extract DC values from NALU (before encryption) for storage
std::vector<int> extract_dc_values_from_nalu(
    const std::vector<uint8_t>& nalu
);

// Save DC values to metadata file
void save_dc_values_to_file(
    const std::string& filename,
    const std::vector<std::vector<int>>& all_dc_values
);
