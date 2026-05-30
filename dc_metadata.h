#pragma once

#include <cstdint>
#include <vector>
#include <string>

/**
 * Extended DC Metadata with Padding Information
 * 
 * Format:
 * - nalu_index: Index of this DC block
 * - y_start, y_count: Y component location and size
 * - cb_start, cb_count: Cb component location and size  
 * - cr_start, cr_count: Cr component location and size
 * - y_padding: Padding byte for Y component (for Arnold 2D 5×5)
 * - cb_padding: Padding byte for Cb component
 * - cr_padding: Padding byte for Cr component
 */

struct DCMetadataWithPadding {
    int nalu_index;
    int y_start, y_count;
    int cb_start, cb_count;
    int cr_start, cr_count;
    uint8_t y_padding;   // Padding byte for Y
    uint8_t cb_padding;  // Padding byte for Cb
    uint8_t cr_padding;  // Padding byte for Cr
    uint8_t extra_byte_encrypted;  // NEW: The 25th encrypted byte (Arnold 2D needs it for inversion)
};

/**
 * Save metadata with padding information
 */
void save_dc_metadata_with_padding(
    const std::string& filename,
    const std::vector<DCMetadataWithPadding>& metadata
);

/**
 * Load metadata with padding information
 */
std::vector<DCMetadataWithPadding> load_dc_metadata_with_padding(
    const std::string& filename
);
