#include "bitio.h"
#include <vector>
#include <cstring>
#include <stdexcept>

BitReader::BitReader(const uint8_t* data, size_t size)
    : data_(data), size_(size), byte_pos_(0), bit_pos_(0) {}

uint32_t BitReader::read_bit() {
    return read_bits(1);
}

uint32_t BitReader::read_bits(int n) {
    uint32_t val = 0;
    for (int i = 0; i < n; ++i) {
        if (byte_pos_ >= size_) return val;
        int bit = (data_[byte_pos_] >> (7 - bit_pos_)) & 1;
        val = (val << 1) | bit;
        bit_pos_++;
        if (bit_pos_ == 8) {
            bit_pos_ = 0;
            byte_pos_++;
        }
    }
    return val;
}

uint32_t BitReader::read_ue_v() {
    int leadingZeroBits = -1;
    for (uint32_t b = 0; b == 0; ) {
        if (byte_pos_ >= size_) {
            // Safety: if we've reached EOF, return 0
            return 0;
        }
        b = read_bits(1);
        leadingZeroBits++;
        if (leadingZeroBits > 31) {
            // Safety: prevent infinite loop on malformed data
            return 0;
        }
    }
    if (leadingZeroBits < 0) return 0;
    uint32_t codeNum = (1u << leadingZeroBits) - 1 + read_bits(leadingZeroBits);
    return codeNum;
}

int BitReader::read_se_v() {
    uint32_t codeNum = read_ue_v();
    if (codeNum == 0) return 0;
    int value = (codeNum + 1) / 2;
    if (codeNum % 2 == 0) value = -value;
    return value;
}

size_t BitReader::bytes_consumed() const {
    return byte_pos_ + (bit_pos_ ? 1 : 0);
}

// BitWriter implementation
BitWriter::BitWriter() : bit_pos_(0) {}

void BitWriter::write_bit(uint32_t bit) {
    if (bit_pos_ == 0) buf_.push_back(0);
    buf_.back() |= (uint8_t)((bit & 1) << (7 - bit_pos_));
    bit_pos_++;
    if (bit_pos_ == 8) bit_pos_ = 0;
}

void BitWriter::write_bits(uint32_t val, int n) {
    for (int i = n - 1; i >= 0; --i) {
        write_bit((val >> i) & 1);
    }
}

void BitWriter::write_ue_v(uint32_t val) {
    if (val == 0) {
        write_bit(1);
        return;
    }
    uint32_t codeNum = val + 1;
    int leading = 31 - __builtin_clz(codeNum);
    for (int i = 0; i < leading; ++i) write_bit(0);
    write_bits(codeNum, leading + 1);
}

void BitWriter::write_se_v(int val) {
    uint32_t codeNum = (val <= 0) ? (-2 * val) : (2 * val - 1);
    write_ue_v(codeNum);
}

void BitWriter::byte_align() {
    while (bit_pos_ != 0) write_bit(0);
}

const std::vector<uint8_t>& BitWriter::data() const { return buf_; }
