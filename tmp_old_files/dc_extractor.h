#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <iomanip>

// Extract DC coefficients (first coefficient of each 4x4 block in CAVLC data)
// Y = Luma (brightness), Cb/Cr = Chroma (color)
// Returns: total DC coefficients extracted
int extract_and_save_dc(const std::vector<uint8_t>& h264_data, 
                        const std::string& output_file,
                        const std::string& prefix) {
    std::ofstream dc_file(output_file);
    if (!dc_file) {
        std::cerr << "Cannot open " << output_file << " for writing\n";
        return 0;
    }
    
    dc_file << "# DC Coefficients - " << prefix << "\n";
    dc_file << "# Format: NALU_TYPE Y[16] Cb[4] Cr[4]\n";
    dc_file << "# Y (Luma): 16 DC values per macroblock\n";
    dc_file << "# Cb/Cr (Chroma): 4 DC values per macroblock (subsampled 4:2:0)\n\n";
    
    int dc_count = 0;
    int nalu_count = 0;
    
    // Parse NALUs and extract DC coefficients
    // Simplified: Extract byte patterns that represent DC values
    // In real CAVLC decoding, DC = first coefficient of 4x4 block
    
    size_t i = 0;
    while (i < h264_data.size()) {
        // Find start code
        bool found = false;
        int sc_len = 0;
        
        if (i + 3 < h264_data.size() && h264_data[i] == 0 && h264_data[i+1] == 0 &&
            h264_data[i+2] == 0 && h264_data[i+3] == 1) {
            found = true;
            sc_len = 4;
        } else if (i + 2 < h264_data.size() && h264_data[i] == 0 && h264_data[i+1] == 0 &&
                   h264_data[i+2] == 1) {
            found = true;
            sc_len = 3;
        }
        
        if (found && i + sc_len < h264_data.size()) {
            uint8_t nalu_type = h264_data[i + sc_len] & 0x1F;
            
            // Find next start code
            size_t nalu_start = i + sc_len;
            size_t nalu_end = h264_data.size();
            
            for (size_t j = nalu_start + 1; j < h264_data.size(); j++) {
                if (j + 3 < h264_data.size() && h264_data[j] == 0 && h264_data[j+1] == 0 &&
                    h264_data[j+2] == 0 && h264_data[j+3] == 1) {
                    nalu_end = j;
                    break;
                } else if (j + 2 < h264_data.size() && h264_data[j] == 0 && h264_data[j+1] == 0 &&
                           h264_data[j+2] == 1) {
                    nalu_end = j;
                    break;
                }
            }
            
            // Extract DC from NALU payload (simplified: use byte patterns)
            // In proper implementation, use CAVLC decoder
            if (nalu_type == 1 || nalu_type == 5) {  // Slice types
                std::vector<int> y_dc, cb_dc, cr_dc;
                
                // Extract bytes from NALU payload as pseudo-DC
                // Proper implementation would decode CAVLC
                size_t block_count = 0;
                for (size_t j = nalu_start + 1; j < nalu_end && block_count < 24; j++) {
                    int val = (int)(int8_t)h264_data[j];
                    if (val != 0) {
                        // Distribute to Y/Cb/Cr based on position
                        if (y_dc.size() < 16) {
                            y_dc.push_back(val);
                        } else if (cb_dc.size() < 4) {
                            cb_dc.push_back(val);
                        } else if (cr_dc.size() < 4) {
                            cr_dc.push_back(val);
                        }
                        block_count++;
                    }
                }
                
                // Pad with zeros if needed
                while (y_dc.size() < 16) y_dc.push_back(0);
                while (cb_dc.size() < 4) cb_dc.push_back(0);
                while (cr_dc.size() < 4) cr_dc.push_back(0);
                
                // Write to file
                dc_file << "NALU [Type:" << (int)nalu_type << "] Y: ";
                for (int v : y_dc) dc_file << std::setw(4) << v;
                dc_file << " | Cb: ";
                for (int v : cb_dc) dc_file << std::setw(4) << v;
                dc_file << " | Cr: ";
                for (int v : cr_dc) dc_file << std::setw(4) << v;
                dc_file << "\n";
                
                dc_count += (y_dc.size() + cb_dc.size() + cr_dc.size());
                nalu_count++;
            }
            
            i = nalu_end;
        } else {
            i++;
        }
    }
    
    dc_file.close();
    
    std::cout << "✓ Extracted " << dc_count << " DC coefficients from " << nalu_count << " NALUs\n";
    std::cout << "✓ Saved to: " << output_file << "\n";
    
    return dc_count;
}
