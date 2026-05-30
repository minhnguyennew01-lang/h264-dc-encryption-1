#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include "encryption.h"
#include "dc_extractor.h"

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <input_h264> <output_h264> <key>\n";
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_file = argv[2];
    std::string key = argv[3];

    std::cout << "[Strategy 3] Reading file...\n";
    std::ifstream infile(input_file, std::ios::binary);
    if (!infile) {
        std::cerr << "Error reading input file\n";
        return 1;
    }
    infile.seekg(0, std::ios::end);
    size_t file_size = infile.tellg();
    infile.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> h264_data(file_size);
    infile.read((char*)h264_data.data(), file_size);
    infile.close();

    std::cout << "[Strategy 3] File size: " << file_size << " bytes\n";

    // Extract DC BEFORE encryption
    std::cout << "[Strategy 3] Extracting DC coefficients BEFORE encryption...\n";
    int dc_count_before = extract_and_save_dc(h264_data, output_file + ".dc_before.txt", "BEFORE");

    // Parse and encrypt NALUs
    std::cout << "[Strategy 3] Extracting NALUs...\n";
    std::vector<std::vector<uint8_t>> nalus;
    std::vector<uint8_t> nalu_types;
    std::vector<size_t> nalu_offsets;
    
    size_t i = 0;
    int nalu_count = 0;
    while (i < h264_data.size()) {
        if (i + 3 <= h264_data.size() && 
            h264_data[i] == 0 && h264_data[i+1] == 0 && h264_data[i+2] == 1) {
            
            size_t start = i + 3;
            uint8_t header = h264_data[start];
            uint8_t nalu_type = header & 0x1F;
            nalu_types.push_back(nalu_type);
            nalu_offsets.push_back(i);
            
            i += 3;
            size_t next_pos = i;
            while (next_pos + 2 < h264_data.size()) {
                if (h264_data[next_pos] == 0 && h264_data[next_pos+1] == 0 && 
                    (h264_data[next_pos+2] == 0 || h264_data[next_pos+2] == 1)) {
                    break;
                }
                next_pos++;
            }
            
            std::vector<uint8_t> nalu(h264_data.begin() + i, h264_data.begin() + next_pos);
            nalus.push_back(nalu);
            i = next_pos;
            nalu_count++;
        } else {
            i++;
        }
    }

    std::cout << "[Strategy 3] Found " << nalu_count << " NALUs\n";

    // Strategy 3: Encrypt I-frames AND P-frames (NALU types 5 and 1)
    std::cout << "[Strategy 3] Encrypting I-frames and P-frames...\n";
    std::vector<uint8_t> key_bytes(key.begin(), key.end());
    int encrypted_count = 0;
    for (size_t j = 0; j < nalus.size(); j++) {
        if (nalu_types[j] == 5 || nalu_types[j] == 1) { // I-frame or P-frame
            nalus[j] = encrypt_dc_coefficients(nalus[j], key_bytes, encrypted_count);  // Use encrypted count for consistent seeding
            encrypted_count++;
        }
    }

    // Reconstruct H.264 file
    std::cout << "[Strategy 3] Writing encrypted output...\n";
    std::vector<uint8_t> output_data;
    std::vector<std::pair<size_t, size_t>> nalu_boundaries;
    
    for (size_t j = 0; j < nalus.size(); j++) {
        size_t start = output_data.size();
        output_data.push_back(0);
        output_data.push_back(0);
        output_data.push_back(1);
        output_data.insert(output_data.end(), nalus[j].begin(), nalus[j].end());
        nalu_boundaries.push_back({start, output_data.size()});
    }

    // Extract DC AFTER encryption
    std::cout << "[Strategy 3] Extracting DC coefficients AFTER encryption...\n";
    int dc_count_after = extract_and_save_dc(output_data, output_file + ".dc_after.txt", "AFTER");

    // Write encrypted file
    std::ofstream outfile(output_file, std::ios::binary);
    outfile.write((char*)output_data.data(), output_data.size());
    outfile.close();

    // Write metadata (binary format for compatibility with decrypt)
    std::string meta_file = output_file + ".meta";
    std::ofstream meta(meta_file, std::ios::binary);
    
    // Write header
    meta.write("META", 4);
    
    // Write key
    uint32_t key_len = key_bytes.size();
    meta.write((char*)&key_len, 4);
    meta.write((char*)key_bytes.data(), key_len);
    
    // Write strategy
    uint32_t strategy = 3;
    meta.write((char*)&strategy, 4);
    
    // Write total NALUs
    uint32_t total = nalus.size();
    meta.write((char*)&total, 4);
    
    // Write NALU boundaries
    for (size_t j = 0; j < nalus.size(); j++) {
        uint32_t nalu_start = nalu_boundaries[j].first;
        uint32_t nalu_len = nalu_boundaries[j].second - nalu_boundaries[j].first;
        uint8_t nalu_type = nalu_types[j];
        
        meta.write((char*)&nalu_start, 4);
        meta.write((char*)&nalu_len, 4);
        meta.write((char*)&nalu_type, 1);
    }
    
    meta.close();

    std::cout << "Encryption complete:\n"
              << "  Total NALUs: " << nalus.size() << "\n"
              << "  Encrypted (I+P-frames): " << encrypted_count 
              << " (" << (100.0 * encrypted_count / nalus.size()) << "%)\n"
              << "  Output file: " << output_file << "\n"
              << "  Output size: " << output_data.size() << " bytes\n"
              << "  Metadata file: " << meta_file << "\n"
              << "  DC before encryption: " << output_file << ".dc_before.txt\n"
              << "  DC after encryption: " << output_file << ".dc_after.txt\n";

    return 0;
}
