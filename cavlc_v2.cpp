#include "cavlc.h"
#include "bitio.h"
#include <algorithm>
#include <iostream>
#include <cstring>
#include <bitset>

using namespace std;

static const int BLOCK_SIZE = 16;

// zig-zag scan order for 4x4 block
static const int ZZ4[16] = {
    0, 1, 4, 8,
    2, 5, 9, 12,
    3, 6, 10, 13,
    7, 11, 14, 15
};

// ============== CAVLC Pragmatic Decoder ==============

// Decode coeff_token - SAFE pragmatic approach
static pair<int,int> decode_coeff_token_safe(BitReader& br) {
    // Decode single ue_v as combined value
    uint32_t val = br.read_ue_v();
    int num_coeff = (val >> 2) & 0xF;
    int trailing_ones = val & 3;
    if (num_coeff > 16) num_coeff = 16;
    if (trailing_ones > 3) trailing_ones = 3;
    return {num_coeff, trailing_ones};
}

// Placeholder: decode total_zeros per spec (table-driven). Fallback to ue_v().
static int decode_total_zeros(BitReader& br, int num_coeff) {
    if (num_coeff == 0) return 0;
    uint32_t val = br.read_ue_v();
    return (val > 15) ? 15 : val;  // Clamp to valid range
}

// Placeholder: decode run_before per spec (table-driven). Fallback to ue_v().
static int decode_run_before(BitReader& br, int zeros_left) {
    if (zeros_left <= 0) return 0;
    uint32_t val = br.read_ue_v();
    return (val > (uint32_t)zeros_left) ? zeros_left : val;  // Clamp to zeros_left
}

vector<int> cavlc_decode_4x4(BitReader& br) {
    // Decode coeff_token SAFELY
    auto [num_coeff, trailing_ones] = decode_coeff_token_safe(br);
    
    if (num_coeff > 16) num_coeff = 16;
    if (trailing_ones > num_coeff) trailing_ones = num_coeff;

    vector<int> coeff(BLOCK_SIZE, 0);
    
    // Decode trailing ones (with signs)
    for (int i = 0; i < trailing_ones; ++i) {
        int sign = br.read_bits(1);
        coeff[i] = (sign == 0) ? 1 : -1;
    }
    
    // Decode levels
    // For now use pragmatic Exp-Golomb level coding (encoder uses write_se_v)
    for (int i = trailing_ones; i < num_coeff; ++i) {
        coeff[i] = br.read_se_v();
    }
    
    int total_zeros = 0;
    if (num_coeff < 16) {
        total_zeros = decode_total_zeros(br, num_coeff);
    }

    // Decode run_before for each non-zero coefficient
    vector<int> runs;
    runs.reserve(num_coeff);
    int zeros_left = total_zeros;
    for (int i = 0; i < num_coeff - 1; ++i) {
        int run = decode_run_before(br, zeros_left);
        runs.push_back(min(run, zeros_left));
        zeros_left -= runs.back();
    }
    if (num_coeff > 0) runs.push_back(zeros_left);

    // Place coefficients in 4x4 block using zig-zag order and runs
    vector<int> out(BLOCK_SIZE, 0);
    int pos = 0;
    for (int i = 0; i < num_coeff; ++i) {
        if (i < (int)runs.size()) {
            pos += runs[i];
        }
        // Safety bounds check
        if (pos >= BLOCK_SIZE) {
            break;
        }
        out[ZZ4[pos]] = coeff[i];
        pos++;
    }

    return out;
}

void cavlc_encode_4x4(BitWriter& bw, const vector<int>& coeffs) {
    // Determine non-zero positions in scan order (pos = index in scan)
    vector<int> nonzero_pos;
    for (int pos = 0; pos < BLOCK_SIZE; ++pos) {
        int idx = ZZ4[pos];
        if (coeffs[idx] != 0) nonzero_pos.push_back(pos);
    }

    int num_coeff = (int)nonzero_pos.size();

    // Build coded values in scan order (non-decreasing positions)
    vector<int> coded_vals;
    coded_vals.reserve(num_coeff);
    for (int k = 0; k < (int)nonzero_pos.size(); ++k) {
        int idx = ZZ4[nonzero_pos[k]];
        coded_vals.push_back(coeffs[idx]);
    }

    // Count leading ones in coded_vals starting from index 0 (matches decoder order)
    int trailing_ones = 0;
    for (int k = 0; k < (int)coded_vals.size() && trailing_ones < 3; ++k) {
        if (coded_vals[k] == 1 || coded_vals[k] == -1) trailing_ones++; else break;
    }

    // Encode coeff_token (using ue_v for pragmatic compatibility)
    bw.write_ue_v(num_coeff);
    bw.write_ue_v(trailing_ones);

    // Encode trailing ones signs (in coded order)
    for (int k = 0; k < trailing_ones; ++k) {
        bw.write_bits(coded_vals[k] < 0 ? 1 : 0, 1);
    }

    // Encode levels for remaining coded values
    for (int k = trailing_ones; k < (int)coded_vals.size(); ++k) {
        bw.write_se_v(coded_vals[k]);
    }

    // Compute runs (run_before) using nonzero_pos (scan order)
    if (num_coeff < BLOCK_SIZE) {
        vector<int> runs;
        runs.reserve(num_coeff);
        int pos = 0;
        for (int k = 0; k < (int)nonzero_pos.size(); ++k) {
            int desired = nonzero_pos[k];
            int run = desired - pos;
            if (run < 0) run = 0;
            runs.push_back(run);
            pos = desired + 1;
        }

        int total_zeros = 0;
        for (int i = 0; i < (int)runs.size(); ++i) total_zeros += runs[i];
        bw.write_ue_v(total_zeros);

        for (int i = 0; i < (int)runs.size() - 1; ++i) {
            bw.write_ue_v(runs[i]);
        }
    }
}

pair<int,int> decode_coeff_token(BitReader& br, int nC) {
    return decode_coeff_token_safe(br);
}

