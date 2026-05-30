#include <iostream>
#include <cstdint>
#include <fstream>
#include <vector>
#include <cstring>
#include "encryption.h"
#include "dc_extractor.h"

// STRATEGY 1 WITH DC EXTRACTION: Encrypt I/P/B video frames only (skip metadata SPS/PPS/SEI)
// PLUS: Extract DC coefficients BEFORE and AFTER encryption
// Preserves NALU boundaries in metadata for perfect decryption

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <input.h264> <output.h264> <key>\n";
        return 1;
    }

    std::string key_str = argv[3];
    std::vector<uint8_t> key_bytes(key_str.begin(), key_str.end());

    std::ifstream input(argv[1], std::ios::binary);
    std::ofstream output(argv[2], std::ios::binary);
    
    if (!input || !output) {
        std::cerr << "Error opening files\n";
        return 1;
    }

    // Read entire file
    std::vector<uint8_t> file_data;
    uint8_t byte;
    
    std::cout << "[Strategy 1] Reading file...\n";
    while (input.read((char*)&byte, 1)) {
        file_data.push_back(byte);
    }
    input.close();
    
    std::cout << "[Strategy 1] File size: " << file_data.size() << " bytes\n";
    
    // ============ EXTRACT DC BEFORE ENCRYPTION ============
    std::cout << "[Strategy 1] Extracting DC coefficients BEFORE encryption...\n";
    std::string dc_before_file = std::string(argv[2]) + ".dc_before.txt";
    extract_and_save_dc(file_data, dc_before_file, "BEFORE ENCRYPTION (Original)");
    
    std::cout << "[Strategy 1] Extracting NALUs...\n";

    // Find all start codes and extract NALUs
    std::vector<std::pair<size_t, size_t>> nalus;  // (start, length)
    std::vector<uint8_t> nalu_types;
    
    size_t i = 0;
    while (i < file_data.size()) {
        bool found_sc = false;
        int sc_len = 0;
        
        // Check 4-byte start code
        if (i + 3 < file_data.size() &&
            file_data[i] == 0x00 && file_data[i+1] == 0x00 &&
            file_data[i+2] == 0x00 && file_data[i+3] == 0x01) {
            found_sc = true;
            sc_len = 4;
        }
        // Check 3-byte start code
        else if (i + 2 < file_data.size() &&
                 file_data[i] == 0x00 && file_data[i+1] == 0x00 &&
                 file_data[i+2] == 0x01 && (i == 0 || file_data[i-1] != 0x00)) {
            found_sc = true;
            sc_len = 3;
        }
        
        if (found_sc) {
            // Record NALU type if we have enough bytes
            if (i + sc_len < file_data.size()) {
                uint8_t nalu_type = file_data[i + sc_len] & 0x1F;
                nalu_types.push_back(nalu_type);
            }
            i += sc_len;
        } else {
            i++;
        }
    }
    
    // Now extract NALU boundaries more carefully
    i = 0;
    size_t nalu_idx = 0;
    
    while (i < file_data.size() && nalu_idx < nalu_types.size()) {
        bool found_sc = false;
        int sc_len = 0;
        size_t sc_start = i;
        
        // Check 4-byte start code
        if (i + 3 < file_data.size() &&
            file_data[i] == 0x00 && file_data[i+1] == 0x00 &&
            file_data[i+2] == 0x00 && file_data[i+3] == 0x01) {
            found_sc = true;
            sc_len = 4;
        }
        // Check 3-byte start code
        else if (i + 2 < file_data.size() &&
                 file_data[i] == 0x00 && file_data[i+1] == 0x00 &&
                 file_data[i+2] == 0x01 && (i == 0 || file_data[i-1] != 0x00)) {
            found_sc = true;
            sc_len = 3;
        }
        
        if (found_sc) {
            // Find next start code
            size_t next_sc = file_data.size();
            for (size_t j = sc_start + sc_len + 1; j < file_data.size(); j++) {
                bool found_next = false;
                
                if (j + 3 < file_data.size() &&
                    file_data[j] == 0x00 && file_data[j+1] == 0x00 &&
                    file_data[j+2] == 0x00 && file_data[j+3] == 0x01) {
                    found_next = true;
                    next_sc = j;
                    break;
                } else if (j + 2 < file_data.size() &&
                           file_data[j] == 0x00 && file_data[j+1] == 0x00 &&
                           file_data[j+2] == 0x01 && (j == 0 || file_data[j-1] != 0x00)) {
                    found_next = true;
                    next_sc = j;
                    break;
                }
            }
            
            size_t nalu_length = next_sc - sc_start;
            nalus.push_back({sc_start, nalu_length});
            
            i = next_sc;
            nalu_idx++;
        } else {
            i++;
        }
    }
    
    std::cout << "[Strategy 1] Found " << nalus.size() << " NALUs\n";

    // Extract NALU as separate vectors (like Strategy 2 & 3)
    std::vector<std::vector<uint8_t>> nalu_vecs;
    for (size_t n = 0; n < nalus.size(); n++) {
        size_t start = nalus[n].first;
        size_t length = nalus[n].second;
        std::vector<uint8_t> nalu(file_data.begin() + start, file_data.begin() + start + length);
        nalu_vecs.push_back(nalu);
    }

    // Encrypt NALUs - Strategy 1: Encrypt ALL frames (including SPS/PPS/SEI)
    std::cout << "[Strategy 1] Encrypting ALL frames...\n";
    int encrypted_count = 0;
    
    for (size_t j = 0; j < nalu_vecs.size(); j++) {
        nalu_vecs[j] = encrypt_dc_coefficients(nalu_vecs[j], key_bytes, encrypted_count);
        encrypted_count++;
    }

    // Reconstruct H.264 file from encrypted NALUs
    std::cout << "[Strategy 1] Writing encrypted output...\n";
    std::vector<uint8_t> output_data;
    for (size_t j = 0; j < nalu_vecs.size(); j++) {
        output_data.insert(output_data.end(), nalu_vecs[j].begin(), nalu_vecs[j].end());
    }
    
    for (uint8_t b : output_data) {
        output.write((char*)&b, 1);
    }
    output.close();
    
    // ============ EXTRACT DC AFTER ENCRYPTION ============
    std::cout << "[Strategy 1] Extracting DC coefficients AFTER encryption...\n";
    std::string dc_after_file = std::string(argv[2]) + ".dc_after.txt";
    extract_and_save_dc(output_data, dc_after_file, "AFTER ENCRYPTION (Strategy 1 Encrypted)");
    
    // Save metadata with NALU boundaries
    std::string meta_file = std::string(argv[2]) + ".meta";
    std::ofstream meta(meta_file, std::ios::binary);
    
    // Write header
    meta.write("META", 4);
    
    // Write key
    uint32_t key_len = key_bytes.size();
    meta.write((char*)&key_len, 4);
    meta.write((char*)key_bytes.data(), key_len);
    
    // Write strategy
    uint32_t strategy = 1;
    meta.write((char*)&strategy, 4);
    
    // Write total NALUs
    uint32_t total = nalus.size();
    meta.write((char*)&total, 4);
    
    // Write NALU boundaries
    for (size_t n = 0; n < nalus.size(); n++) {
        uint32_t nalu_start = nalus[n].first;
        uint32_t nalu_len = nalus[n].second;
        uint8_t nalu_type = nalu_types[n];
        
        meta.write((char*)&nalu_start, 4);
        meta.write((char*)&nalu_len, 4);
        meta.write((char*)&nalu_type, 1);
    }
    
    meta.close();
    
    int total_encrypted = iframe_count + pframe_count;
    std::cout << "Encryption complete:\n";
    std::cout << "  Total NALUs: " << nalus.size() << "\n";
    std::cout << "  Encrypted (I/P/B): " << total_encrypted << " (" 
              << (100.0 * total_encrypted / nalus.size()) << "%)\n";
    std::cout << "  Output file: " << argv[2] << "\n";
    std::cout << "  Output size: " << output_data.size() << " bytes\n";
    std::cout << "  Metadata file: " << meta_file << "\n";
    std::cout << "  DC before encryption: " << dc_before_file << "\n";
    std::cout << "  DC after encryption: " << dc_after_file << "\n";

    return 0;
}
