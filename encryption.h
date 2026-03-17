#pragma once

#include <cstdint>
#include <vector>

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
    const std::vector<uint8_t>& key
);

std::vector<uint8_t> decrypt_dc_coefficients(
    const std::vector<uint8_t>& nalu,
    const std::vector<uint8_t>& key
);
