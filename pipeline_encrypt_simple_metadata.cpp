#include <iostream>
#include <cstdint>
#include <fstream>
#include <vector>
#include <cstring>
#include "encryption.h"
#include "rbsp.h"

struct NALUMetadata {
    uint32_t nalu_index;
    uint32_t original_size;
    uint32_t encrypted_size;
    uint8_t nalu_type;
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <input.h264> <output.h264> <key>\n";
        return 1;
    }

    std::string key_str = argv[3];
    std::vector<uint8_t> key_bytes(key_str.begin(), key_str.end());

    std::ifstream input(argv[1], std::ios::binary);
    std::ofstream output(argv[2], std::ios::binary);
    
    std::string metadata_file = std::string(argv[2]) + ".meta";
    std::ofstream metadata(metadata_file, std::ios::binary);

    if (!input || !output || !metadata) {
        std::cerr << "Error opening files\n";
        return 1;
    }

    // Write metadata header
    uint8_t meta_header[] = {'M', 'E', 'T', 'A'};
    metadata.write((char*)meta_header, 4);
    
    // Write key length and key
    uint32_t key_len = key_bytes.size();
    metadata.write((char*)&key_len, 4);
    metadata.write((char*)key_bytes.data(), key_len);

    std::vector<uint8_t> nalu;
    std::vector<NALUMetadata> nalu_metadata;
    int nalu_count = 0;

    std::cout << "[1/5] Reading NALUs\n";

    while (input) {
        uint8_t byte;
        input.read((char*)&byte, 1);
        if (!input) break;

        nalu.push_back(byte);

        // Check for next start code
        if (nalu.size() >= 4) {
            if (nalu[nalu.size()-4] == 0x00 && nalu[nalu.size()-3] == 0x00 &&
                nalu[nalu.size()-2] == 0x00 && nalu[nalu.size()-1] == 0x01) {
                // Found next NALU, process previous one
                if (nalu.size() > 8) {
                    std::vector<uint8_t> prev_nalu(nalu.begin(), nalu.end() - 4);
                    
                    nalu.clear();
                    nalu.push_back(0x00);
                    nalu.push_back(0x00);
                    nalu.push_back(0x00);
                    nalu.push_back(0x01);

                    nalu_count++;
                    if (nalu_count % 1000 == 0) {
                        std::cout << "  NALU " << nalu_count << " processed\n";
                    }

                    uint32_t original_size = prev_nalu.size();
                    uint8_t nalu_type = (prev_nalu[4] & 0x1F);

                    // Perform actual DC coefficient encryption
                    std::vector<uint8_t> encrypted_nalu = encrypt_dc_coefficients(prev_nalu, key_bytes);

                    uint32_t encrypted_size = encrypted_nalu.size();

                    // Write NALU to output
                    output.write((char*)encrypted_nalu.data(), encrypted_nalu.size());

                    // Save metadata
                    NALUMetadata meta;
                    meta.nalu_index = nalu_count - 1;
                    meta.original_size = original_size;
                    meta.encrypted_size = encrypted_size;
                    meta.nalu_type = nalu_type;
                    nalu_metadata.push_back(meta);
                }
            }
        }
    }

    // Handle last NALU
    if (nalu.size() > 4) {
        nalu_count++;
        uint32_t original_size = nalu.size();
        uint8_t nalu_type = (nalu[4] & 0x1F);

        std::vector<uint8_t> encrypted_nalu = encrypt_dc_coefficients(nalu, key_bytes);
        output.write((char*)encrypted_nalu.data(), encrypted_nalu.size());

        NALUMetadata meta;
        meta.nalu_index = nalu_count - 1;
        meta.original_size = original_size;
        meta.encrypted_size = encrypted_nalu.size();
        meta.nalu_type = nalu_type;
        nalu_metadata.push_back(meta);
    }

    std::cout << "[2/5] Writing metadata\n";

    // Write number of NALUs
    uint32_t num_nalus = nalu_metadata.size();
    metadata.write((char*)&num_nalus, 4);

    // Write metadata for each NALU
    for (const auto& meta : nalu_metadata) {
        metadata.write((char*)&meta.nalu_index, 4);
        metadata.write((char*)&meta.original_size, 4);
        metadata.write((char*)&meta.encrypted_size, 4);
        metadata.write((char*)&meta.nalu_type, 1);
    }

    // Write footer
    uint8_t meta_footer[] = {'E', 'N', 'D', 'M'};
    metadata.write((char*)meta_footer, 4);

    std::cout << "[3/5] Processed NALUs: " << nalu_count << "\n";
    std::cout << "[4/5] Wrote encrypted file: " << argv[2] << "\n";
    std::cout << "[5/5] Wrote metadata file: " << metadata_file << "\n\n";
    
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  METADATA STORAGE SUMMARY                                  ║\n";
    std::cout << "├────────────────────────────────────────────────────────────┤\n";
    std::cout << "║  Encrypted file:          " << argv[2] << "\n";
    std::cout << "║  Metadata file:           " << metadata_file << "\n";
    std::cout << "║  Total NALUs:             " << num_nalus << "\n";
    std::cout << "║  Key:                     " << key_str << "\n";
    std::cout << "║  Status:                  ✓ SUCCESS\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    input.close();
    output.close();
    metadata.close();

    return 0;
}
