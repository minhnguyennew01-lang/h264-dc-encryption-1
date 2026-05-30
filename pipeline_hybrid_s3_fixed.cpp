#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include "hybrid_encryption.h"
#include "dc_metadata.h"

using namespace std;

// Rigorously handle emulation prevention:
// - Input: NALU payload WITH emulation prevention bytes
// - Output: NALU payload ready to write to file WITH proper emulation prevention

class NALUPayloadHandler {
public:
    // Remove emulation prevention bytes (0x00 0x00 0x03 sequences)
    // Returns the "clean" payload without emulation prevention
    static vector<uint8_t> remove_emulation_prevention(const vector<uint8_t>& payload) {
        vector<uint8_t> clean;
        
        for (size_t i = 0; i < payload.size(); i++) {
            if (i + 2 < payload.size() &&
                payload[i] == 0x00 && payload[i+1] == 0x00 && 
                payload[i+2] == 0x03) {
                // Found emulation prevention: skip the 0x03, keep the two 0x00s
                clean.push_back(0x00);
                clean.push_back(0x00);
                i += 2;  // Skip the 0x03
            } else {
                clean.push_back(payload[i]);
            }
        }
        
        return clean;
    }
    
    // Insert emulation prevention bytes where needed
    // Rule: Insert 0x03 after 0x00 0x00 if the next byte would be 0x00, 0x01, 0x02, or 0x03
    static vector<uint8_t> insert_emulation_prevention(const vector<uint8_t>& data) {
        vector<uint8_t> result;
        
        for (size_t i = 0; i < data.size(); i++) {
            result.push_back(data[i]);
            
            if (i + 1 < data.size() && 
                result.size() >= 2 &&
                result[result.size()-2] == 0x00 &&
                result[result.size()-1] == 0x00) {
                
                // We just added a second 0x00. Check next byte in original data
                uint8_t next_byte = data[i+1];
                if (next_byte <= 0x03) {
                    // Need emulation prevention
                    result.push_back(0x03);
                }
            }
        }
        
        return result;
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <input.h264> <key>\n";
        cerr << "Output: <input.h264>.s3_hybrid_fixed\n";
        return 1;
    }

    string input_file = argv[1];
    string key_str = argv[2];
    string output_file = input_file + ".s3_hybrid_fixed";
    
    vector<uint8_t> key_bytes(key_str.begin(), key_str.end());

    ifstream input(input_file, ios::binary);
    if (!input) {
        cerr << "❌ Error: Cannot open input file: " << input_file << "\n";
        return 1;
    }
    
    vector<uint8_t> file_data((istreambuf_iterator<char>(input)), 
                              istreambuf_iterator<char>());
    input.close();

    cout << "📄 Input file: " << input_file << " (" << file_data.size() << " bytes)\n";
    cout << "🔑 Key: " << key_str << "\n";
    cout << "📊 Strategy: S3 HYBRID fixed (mixed frequency - every 3rd frame)\n";

    // Find all NALU boundaries (both 3-byte and 4-byte start codes)
    vector<pair<size_t, int>> nalu_positions;  // file_offset, type
    
    for (size_t i = 0; i < file_data.size() - 2; i++) {
        size_t nal_offset = -1;
        
        // 4-byte start code
        if (i + 3 < file_data.size() && 
            file_data[i] == 0x00 && file_data[i+1] == 0x00 && 
            file_data[i+2] == 0x00 && file_data[i+3] == 0x01) {
            nal_offset = i + 4;
        }
        // 3-byte start code (check it's not part of 4-byte)
        else if (i + 2 < file_data.size() &&
                 file_data[i] == 0x00 && file_data[i+1] == 0x00 && 
                 file_data[i+2] == 0x01 &&
                 (i == 0 || file_data[i-1] != 0x00)) {
            nal_offset = i + 3;
        }
        
        if (nal_offset > 0 && nal_offset < file_data.size()) {
            uint8_t nal_byte = file_data[nal_offset];
            int nal_type = nal_byte & 0x1F;
            nalu_positions.push_back({nal_offset, nal_type});
        }
    }
    
    cout << "🔍 Found " << nalu_positions.size() << " NAL units (start codes)\n";
    
    vector<uint8_t> encrypted_file_data = file_data;
    int encrypted_count = 0;
    vector<int> encrypted_nalu_indices;  // Track which NALUs were encrypted
    int frame_counter = 0;  // Count frames for every 3rd logic
    
    cout << "\n🔐 Processing with emulation prevention handling...\n";
    cout << "📌 Strategy S3: Encrypt every 3rd frame (counter % 3 == 0)\n";
    
    // For each NALU, extract, process, and reconstruct
    for (size_t i = 0; i < nalu_positions.size(); i++) {
        size_t nal_start_offset = nalu_positions[i].first;
        int nal_type = nalu_positions[i].second;
        
        // Find NALU boundaries
        size_t nalu_payload_start = nal_start_offset;
        size_t nalu_payload_end = file_data.size();
        
        // Find next NALU
        if (i + 1 < nalu_positions.size()) {
            // Next NALU starts at some start code. Find it.
            // We need to search backward from nalu_positions[i+1].first to find the start code
            size_t next_nal_byte_pos = nalu_positions[i+1].first;
            // The start code is before next_nal_byte_pos
            // 4-byte: nal_byte_pos - 4
            // 3-byte: nal_byte_pos - 3
            
            // Search for start code
            for (size_t j = next_nal_byte_pos - 1; j >= next_nal_byte_pos - 4 && j < file_data.size(); j--) {
                if (j + 3 < file_data.size() && 
                    file_data[j] == 0x00 && file_data[j+1] == 0x00 && 
                    file_data[j+2] == 0x00 && file_data[j+3] == 0x01) {
                    nalu_payload_end = j;
                    break;
                } else if (j + 2 < file_data.size() &&
                          file_data[j] == 0x00 && file_data[j+1] == 0x00 && 
                          file_data[j+2] == 0x01 &&
                          (j == 0 || file_data[j-1] != 0x00)) {
                    nalu_payload_end = j;
                    break;
                }
            }
        }
        
        // Skip SPS/PPS/SEI - don't encrypt
        if (nal_type == 7 || nal_type == 8 || nal_type == 6) {
            continue;
        }
        
        // S3 Strategy: Encrypt every 3rd frame (Type 1 and Type 5)
        bool should_encrypt = false;
        if (nal_type == 1 || nal_type == 5) {  // Type 1 (P/B) or Type 5 (IDR)
            frame_counter++;
            if (frame_counter % 3 == 0) {  // Every 3rd frame
                should_encrypt = true;
            }
        }
        
        if (!should_encrypt) {
            continue;
        }
        
        // Extract NALU payload (with emulation prevention bytes still in place)
        vector<uint8_t> nalu_raw(file_data.begin() + nalu_payload_start,
                                file_data.begin() + nalu_payload_end);
        
        // Remove emulation prevention to get clean data
        vector<uint8_t> nalu_clean = NALUPayloadHandler::remove_emulation_prevention(nalu_raw);
        
        // Now encrypt DC at offset 20 in CLEAN payload
        const int DC_OFFSET = 19;
        const int DC_SIZE = 25;
        
        if (nalu_clean.size() < DC_OFFSET + DC_SIZE) {
            continue;  // Too small
        }
        
        // Extract DC
        vector<uint8_t> dc_data(nalu_clean.begin() + DC_OFFSET,
                               nalu_clean.begin() + DC_OFFSET + DC_SIZE);
        
        // Encrypt
        uint8_t padding_byte = 0;
        vector<uint8_t> dc_encrypted_full = HybridEncryption::encrypt_hybrid(
            dc_data, key_bytes, (int)i, padding_byte);
        
        vector<uint8_t> dc_encrypted(dc_encrypted_full.begin(),
                                     dc_encrypted_full.begin() + DC_SIZE);
        
        // Replace in clean payload
        vector<uint8_t> nalu_clean_encrypted = nalu_clean;
        memcpy(&nalu_clean_encrypted[DC_OFFSET], dc_encrypted.data(), DC_SIZE);
        
        // Re-insert emulation prevention
        vector<uint8_t> nalu_reencoded = NALUPayloadHandler::insert_emulation_prevention(nalu_clean_encrypted);
        
        // IMPORTANT: Only encrypt if the re-encoded size matches the original size!
        // Otherwise the file structure is corrupted
        if (nalu_payload_end - nalu_payload_start != nalu_reencoded.size()) {
            continue;  // Size mismatch - skip this NALU
        }
        
        // Size matches - safe to replace
        memcpy(&encrypted_file_data[nalu_payload_start], nalu_reencoded.data(), nalu_reencoded.size());
        
        encrypted_count++;
        encrypted_nalu_indices.push_back(i);  // Record this NALU was encrypted
        
        if ((i + 1) % 5000 == 0) {
            cout << "  Processed " << (i + 1) << " NALUs...\n";
        }
    }

    ofstream output(output_file, ios::binary);
    output.write((char*)encrypted_file_data.data(), encrypted_file_data.size());
    output.close();

    // Save metadata for decrypt pipelines
    string meta_file = output_file + ".meta";
    ofstream meta(meta_file, ios::binary);
    meta.write("S3FX", 4);  // Strategy 3 Fixed
    for (int idx : encrypted_nalu_indices) {
        uint32_t nalu_idx = idx;
        meta.write((char*)&nalu_idx, 4);
    }
    meta.close();

    cout << "\n✅ S3 Fixed encryption complete!\n";
    cout << "  📊 Total NALUs: " << nalu_positions.size() << "\n";
    cout << "  🔐 Encrypted (every 3rd frame): " << encrypted_count << "\n";
    cout << "  📄 Output file: " << output_file << " (" << encrypted_file_data.size() << " bytes)\n";
    cout << "  📋 Metadata file: " << meta_file << "\n";
    
    return 0;
}
