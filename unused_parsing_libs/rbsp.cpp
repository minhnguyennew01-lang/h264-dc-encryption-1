#include "rbsp.h"
#include <cstddef>

std::vector<uint8_t> rbsp_from_ebsp(const std::vector<uint8_t>& ebsp) {
    std::vector<uint8_t> rbsp;
    rbsp.reserve(ebsp.size());
    size_t i = 0;
    while (i < ebsp.size()) {
        // copy byte
        uint8_t b = ebsp[i++];
        // if we see 0x00 0x00 0x03, drop the 0x03
        if (b == 0x00 && i + 1 < ebsp.size() && ebsp[i] == 0x00 && ebsp[i+1] == 0x03) {
            rbsp.push_back(b);
            // copy next 0x00
            rbsp.push_back(ebsp[i++]);
            // skip the 0x03
            i++; // skip emulation prevention 0x03
            continue;
        }
        rbsp.push_back(b);
    }
    return rbsp;
}

// Insert emulation prevention bytes into RBSP to form EBSP
std::vector<uint8_t> ebsp_from_rbsp(const std::vector<uint8_t>& rbsp) {
    std::vector<uint8_t> ebsp;
    ebsp.reserve(rbsp.size() + 16);
    int zero_count = 0;
    for (size_t i = 0; i < rbsp.size(); ++i) {
        uint8_t b = rbsp[i];
        if (b == 0x00) {
            zero_count++;
        } else {
            if (zero_count >= 2 && (b == 0x00 || b == 0x01 || b == 0x02 || b == 0x03)) {
                // insert 0x03 after two preceding zeros
                ebsp.push_back(0x03);
            }
            zero_count = 0;
        }
        ebsp.push_back(b);
    }
    return ebsp;
}
