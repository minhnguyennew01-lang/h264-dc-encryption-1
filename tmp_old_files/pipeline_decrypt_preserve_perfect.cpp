#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>
#include "encryption.h"

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <encrypted.h264> <original.h264> <output.h264> <key>\n";
        std::cerr << "       Decrypts while preserving byte-perfect match with original\n";
        return 1;
    }

    std::string key_str = argv[4];
    std::vector<uint8_t> key_bytes(key_str.begin(), key_str.end());

    // Read metadata
    std::string meta_file = std::string(argv[1]) + ".meta";
    std::ifstream metadata(meta_file, std::ios::binary);
    if (!metadata) {
        std::cerr << "Error: Metadata file not found: " << meta_file << "\n";
        return 1;
    }

    uint8_t meta_header[4];
    metadata.read((char*)meta_header, 4);
    if (meta_header[0] != 'M' || meta_header[1] != 'E' || 
        meta_header[2] != 'T' || meta_header[3] != 'A') {
        std::cerr << "Error: Invalid metadata header\n";
        return 1;
    }

    uint32_t stored_key_len;
    metadata.read((char*)&stored_key_len, 4);
    std::vector<uint8_t> stored_key(stored_key_len);
    metadata.read((char*)stored_key.data(), stored_key_len);

    uint32_t strategy;
    metadata.read((char*)&strategy, 4);
    
    uint32_t total_nalus;
    metadata.read((char*)&total_nalus, 4);
    
    // Read NALU information
    std::vector<uint32_t> nalu_starts, nalu_lengths;
    std::vector<uint8_t> nalu_types;
    
    for (uint32_t i = 0; i < total_nalus; i++) {
        uint32_t start, length;
        uint8_t type;
        metadata.read((char*)&start, 4);
        metadata.read((char*)&length, 4);
        metadata.read((char*)&type, 1);
        
        nalu_starts.push_back(start);
        nalu_lengths.push_back(length);
        nalu_types.push_back(type);
    }
    
    metadata.close();

    // Read encrypted file
    std::ifstream encrypted_file(argv[1], std::ios::binary);
    if (!encrypted_file) {
        std::cerr << "Error opening encrypted file\n";
        return 1;
    }

    std::vector<uint8_t> encrypted_data;
    uint8_t byte;
    while (encrypted_file.read((char*)&byte, 1)) {
        encrypted_data.push_back(byte);
    }
    encrypted_file.close();

    std::cout << "[1/3] Processing " << total_nalus << " NALUs...\n";
    std::cout << "      Strategy: " << strategy << "\n";

    // Process each NALU
    std::vector<uint8_t> output_data = encrypted_data;  // Copy encrypted data
    int decrypted_count = 0;

    for (uint32_t i = 0; i < total_nalus; i++) {
        uint32_t start = nalu_starts[i];
        uint32_t length = nalu_lengths[i];
        uint8_t nalu_type = nalu_types[i];
        
        if (start + length > encrypted_data.size()) {
            std::cerr << "Error: Invalid NALU bounds at " << i << "\n";
            return 1;
        }

        // Determine if should decrypt
        bool should_decrypt = false;
        if (strategy == 1) {
            should_decrypt = true;  // Decrypt ALL NALUs for Strategy 1
        } else if (strategy == 2) {
            should_decrypt = (nalu_type == 5);
        } else if (strategy == 3) {
            should_decrypt = (nalu_type == 1 || nalu_type == 5);
        }

        if (should_decrypt && length > 1) {
            // Detect start code length
            int sc_len = 3;
            if (length >= 4 && output_data[start] == 0x00 && output_data[start+1] == 0x00 &&
                output_data[start+2] == 0x00 && output_data[start+3] == 0x01) {
                sc_len = 4;
            }

            // Decrypt payload in-place (keep start code and NALU header)
            if (length > (uint32_t)sc_len + 1) {
                KeystreamGenerator ks;
                std::vector<uint8_t> keystream(length - sc_len - 1);
                ks.generateKeystream(key_bytes, keystream.size(), keystream);

                // XOR chaining decryption: P[i] = C[i] ^ K[i] ^ C[i-1]
                uint8_t prev_cipher = output_data[start + sc_len];  // Previous byte (NALU header)
                for (uint32_t j = sc_len + 1; j < length; j++) {
                    uint8_t cipher = output_data[start + j];
                    output_data[start + j] = cipher ^ keystream[j - sc_len - 1] ^ prev_cipher;
                    prev_cipher = cipher;  // Use original ciphertext for chaining
                }
                
                decrypted_count++;
            }
        }
    }

    // Write output
    std::cout << "[2/3] Writing output...\n";
    std::ofstream output_file(argv[3], std::ios::binary);
    if (!output_file) {
        std::cerr << "Error opening output file\n";
        return 1;
    }

    for (uint8_t b : output_data) {
        output_file.write((char*)&b, 1);
    }
    output_file.close();

    std::cout << "[3/3] Verification...\n";
    std::cout << "Decryption complete:\n";
    std::cout << "  Total NALUs: " << total_nalus << "\n";
    std::cout << "  Decrypted: " << decrypted_count << "\n";
    std::cout << "  Output file: " << argv[3] << "\n";
    std::cout << "  Output size: " << output_data.size() << " bytes\n";

    // Compare with original
    std::ifstream original(argv[2], std::ios::binary);
    if (original) {
        std::vector<uint8_t> original_data;
        while (original.read((char*)&byte, 1)) {
            original_data.push_back(byte);
        }
        original.close();
        
        if (original_data.size() == output_data.size()) {
            bool match = true;
            size_t first_diff = 0;
            for (size_t i = 0; i < original_data.size(); i++) {
                if (original_data[i] != output_data[i]) {
                    match = false;
                    first_diff = i;
                    break;
                }
            }
            if (match) {
                std::cout << "  ✓ PERFECT MATCH with original file!\n";
            } else {
                std::cout << "  ✗ File differs from original at byte " << first_diff << "\n";
            }
        } else {
            std::cout << "  ✗ Size mismatch: original " << original_data.size() 
                      << " vs output " << output_data.size() << "\n";
        }
    }

    return 0;
}
