#pragma once

#include <vector>
#include <cstdint>

// Remove emulation prevention bytes from RBSP payload
std::vector<uint8_t> rbsp_from_ebsp(const std::vector<uint8_t>& ebsp);
// Insert emulation prevention bytes (0x03) into RBSP to produce EBSP payload
std::vector<uint8_t> ebsp_from_rbsp(const std::vector<uint8_t>& rbsp);
