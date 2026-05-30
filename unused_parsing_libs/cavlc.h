#pragma once
#include <vector>
#include <cstdint>

// Decode a single 4x4 luma residual block from BitReader (CAVLC)
// Returns 16 coefficients in scan order
std::vector<int> cavlc_decode_4x4(class BitReader& br);

// Encode a single 4x4 luma residual block into BitWriter (CAVLC)
// Input: coeffs (16 entries)
void cavlc_encode_4x4(class BitWriter& bw, const std::vector<int>& coeffs);

// Decode coeff_token (returns pair<num_coeff, trailing_ones>)
std::pair<int,int> decode_coeff_token(class BitReader& br, int nC);
