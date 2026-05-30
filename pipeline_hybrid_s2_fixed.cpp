#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include "hybrid_encryption.h"
#include "dc_metadata.h"

using namespace std;

// Emulation prevention handler
class NALUPayloadHandler {
public:
    static vector<uint8_t> remove_emulation_prevention(const vector<uint8_t>& payload) {
        vector<uint8_t> clean;
        for (size_t i = 0; i < payload.size(); i++) {
            if (i + 2 < payload.size() &&
                payload[i] == 0x00 && payload[i+1] == 0x00 && 
                payload[i+2] == 0x03) {
                clean.push_back(0x00);
                clean.push_back(0x00);
                i += 2;
            } else {
                clean.push_back(payload[i]);
            }
        }
        return clean;
    }
    
    static vector<uint8_t> insert_emulation_prevention(const vector<uint8_t>& data) {
        vector<uint8_t> result;
        for (size_t i = 0; i < data.size(); i++) {
            result.push_back(data[i]);
            if (i + 1 < data.size() && 
                result.size() >= 2 &&
                result[result.size()-2] == 0x00 &&
                result[result.size()-1] == 0x00) {
                uint8_t next_byte = data[i+1];
                if (next_byte <= 0x03) {
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
        cerr << "Output: <input.h264>.s2_hybrid_fixed\n";
        return 1;
    }

    string input_file = argv[1];
    string key_str = argv[2];
    string output_file = input_file + ".s2_hybrid_fixed";
    
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
    cout << "📊 Strategy: HYBRID fixed (proper emulation prevention)\n";
    cout << "📋 Mode: I-frames ONLY (Type 5) - Strategy 2\n";

    // Find all NALU boundaries (both 3-byte and 4-byte start codes)
    vector<pair<size_t, int>> nalu_positions;
    
    for (size_t i = 0; i < file_data.size() - 2; i++) {
        size_t nal_offset = -1;
        
        if (i + 3 < file_data.size() && 
            file_data[i] == 0x00 && file_data[i+1] == 0x00 && 
            file_data[i+2] == 0x00 && file_data[i+3] == 0x01) {
            nal_offset = i + 4;
        }
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
    
    cout << "🔍 Found " << nalu_positions.size() << " NAL units\n";
    
    vector<uint8_t> encrypted_file_data = file_data;
    int encrypted_count = 0;
    vector<int> encrypted_nalu_indices;  // Track which NALUs were encrypted
    int skipped_count = 0;
    
    cout << "\n🔐 Processing (Strategy 2: I-frames only)...\n";
    
    for (size_t i = 0; i < nalu_positions.size(); i++) {
        size_t nal_start_offset = nalu_positions[i].first;
        int nal_type = nalu_positions[i].second;
        
        // STRATEGY 2: Only encrypt Type 5 (IDR/I-frames)
        if (nal_type != 5) {
            skipped_count++;
            continue;
        }
        
        // Find NALU boundaries
        size_t nalu_payload_start = nal_start_offset;
        size_t nalu_payload_end = file_data.size();
        
        if (i + 1 < nalu_positions.size()) {
            size_t next_nal_byte_pos = nalu_positions[i+1].first;
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
        
        vector<uint8_t> nalu_raw(file_data.begin() + nalu_payload_start,
                                file_data.begin() + nalu_payload_end);
        
        vector<uint8_t> nalu_clean = NALUPayloadHandler::remove_emulation_prevention(nalu_raw);
        
        const int DC_OFFSET = 20;
        const int DC_SIZE = 24;
        
        if (nalu_clean.size() < DC_OFFSET + DC_SIZE) {
            skipped_count++;
            continue;
        }
        
        vector<uint8_t> dc_data(nalu_clean.begin() + DC_OFFSET,
                               nalu_clean.begin() + DC_OFFSET + DC_SIZE);
        
        uint8_t padding_byte = 0;
        vector<uint8_t> dc_encrypted_full = HybridEncryption::encrypt_hybrid(
            dc_data, key_bytes, (int)i, padding_byte);
        
        vector<uint8_t> dc_encrypted(dc_encrypted_full.begin(),
                                     dc_encrypted_full.begin() + DC_SIZE);
        
        vector<uint8_t> nalu_clean_encrypted = nalu_clean;
        memcpy(&nalu_clean_encrypted[DC_OFFSET], dc_encrypted.data(), DC_SIZE);
        
        vector<uint8_t> nalu_reencoded = NALUPayloadHandler::insert_emulation_prevention(nalu_clean_encrypted);
        
        if (nalu_payload_end - nalu_payload_start != nalu_reencoded.size()) {
            skipped_count++;
            continue;
        }
        
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
    meta.write("S2FX", 4);  // Strategy 2 Fixed
    for (int idx : encrypted_nalu_indices) {
        uint32_t nalu_idx = idx;
        meta.write((char*)&nalu_idx, 4);
    }
    meta.close();

    cout << "\n✅ Strategy 2 Encryption complete!\n";
    cout << "  📊 Total NALUs: " << nalu_positions.size() << "\n";
    cout << "  🔐 Encrypted (Type 5 only): " << encrypted_count << "\n";
    cout << "  ⏭️  Skipped (others): " << skipped_count << "\n";
    cout << "  📄 Output file: " << output_file << " (" << encrypted_file_data.size() << " bytes)\n";
    cout << "  📋 Metadata file: " << meta_file << "\n";
    
    return 0;
}
