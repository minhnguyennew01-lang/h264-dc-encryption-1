#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
// Đọc bit từ buffer
class BitReader {
public:
    BitReader(const uint8_t* data, size_t size);
    uint32_t read_bits(int n);      // Read n bit
    uint32_t read_bit();            // Read 1 bit
    uint32_t read_ue_v();           // Read exp-golomb unsigned 
    int read_se_v();                // Read exp-golomb signed
    size_t bytes_consumed() const;  // Bytes consumed
private:
    const uint8_t* data_; //buffer input
    size_t size_;       //  buffer size
    size_t byte_pos_;   //  byte position
    int bit_pos_;       //  bit position
};
// Write bit
class BitWriter {
public:
    BitWriter();                        
    void write_bit(uint32_t bit);               // Write 1 bit
    void write_bits(uint32_t val, int n);       // Write n bit
    void write_ue_v(uint32_t val);              // Write exp-golomb unsigned
    void write_se_v(int val);                   // Write exp-golomb signed
    void byte_align();                          // Byte align   
    const std::vector<uint8_t>& data() const;
private:
    std::vector<uint8_t> buf_;      // buffer
    int bit_pos_;                   // bit position
};
