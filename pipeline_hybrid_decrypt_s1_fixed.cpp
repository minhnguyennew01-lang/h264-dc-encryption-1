#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <cstring>
#include "hybrid_encryption.h"

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
        cerr << "Usage: " << argv[0] << " <encrypted.h264.s1_hybrid_fixed> <key>\n";
        return 1;
    }

    string encrypted_file = argv[1];
    string key_str = argv[2];
    string output_file = encrypted_file + ".decrypted";
    string meta_file = encrypted_file + ".meta";
    
    vector<uint8_t> key_bytes(key_str.begin(), key_str.end());

    // Read metadata
    ifstream meta_input(meta_file, ios::binary);
    if (!meta_input) {
        cerr << "❌ Error: Cannot open metadata file: " << meta_file << "\n";
        return 1;
    }
    
    char magic[4];
    meta_input.read(magic, 4);
    if (string(magic, 4) != "S1FX") {
        cerr << "❌ Error: Metadata not for S1\n";
        return 1;
    }
    
    set<int> encrypted_nalu_indices;
    uint32_t nalu_idx;
    while (meta_input.read((char*)&nalu_idx, 4)) {
        encrypted_nalu_indices.insert(nalu_idx);
    }
    meta_input.close();

    ifstream input(encrypted_file, ios::binary);
    if (!input) {
        cerr << "❌ Error: Cannot open encrypted file: " << encrypted_file << "\n";
        return 1;
    }
    
    vector<uint8_t> file_data((istreambuf_iterator<char>(input)), 
                              istreambuf_iterator<char>());
    input.close();

    cout << "📄 Encrypted file: " << encrypted_file << " (" << file_data.size() << " bytes)\n";
    cout << "🔑 Key: " << key_str << "\n";
    cout << "📊 Strategy: HYBRID fixed S1 (using metadata)\n";
    cout << "📋 Mode: Decrypt ALL frames - Strategy 1\n";

    // Find all NALU boundaries
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
    cout << "📋 Metadata contains " << encrypted_nalu_indices.size() << " encrypted NALUs\n";
    
    vector<uint8_t> decrypted_file_data = file_data;
    int decrypted_count = 0;
    
    cout << "\n🔐 Processing Decryption (using metadata + emulation prevention)...\n";
    
    for (size_t i = 0; i < nalu_positions.size(); i++) {
        if (encrypted_nalu_indices.find((int)i) == encrypted_nalu_indices.end()) {
            continue;
        }
        
        size_t nal_start_offset = nalu_positions[i].first;
        
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
        
        // Remove emulation prevention
        vector<uint8_t> nalu_clean = NALUPayloadHandler::remove_emulation_prevention(nalu_raw);
        
        const int DC_OFFSET = 20;
        const int DC_SIZE = 24;
        
        if (nalu_clean.size() < DC_OFFSET + DC_SIZE) {
            continue;
        }
        
        vector<uint8_t> dc_encrypted(nalu_clean.begin() + DC_OFFSET,
                                     nalu_clean.begin() + DC_OFFSET + DC_SIZE);
        
        uint8_t padding_byte = 0;
        vector<uint8_t> dc_encrypted_padded = dc_encrypted;
        dc_encrypted_padded.push_back(0);
        
        vector<uint8_t> dc_decrypted = HybridEncryption::decrypt_hybrid(
            dc_encrypted_padded, key_bytes, (int)i, padding_byte);
        
        vector<uint8_t> dc_decrypted_24(dc_decrypted.begin(), 
                                        dc_decrypted.begin() + min((size_t)24, dc_decrypted.size()));
        
        if (dc_decrypted_24.size() != 24) {
            continue;
        }
        
        // Replace in clean payload
        vector<uint8_t> nalu_clean_decrypted = nalu_clean;
        memcpy(&nalu_clean_decrypted[DC_OFFSET], dc_decrypted_24.data(), DC_SIZE);
        
        // Re-insert emulation prevention
        vector<uint8_t> nalu_reencoded = NALUPayloadHandler::insert_emulation_prevention(nalu_clean_decrypted);
        
        // Check size matches (should if encrypt only modified size-preserving NALUs)
        if (nalu_payload_end - nalu_payload_start != nalu_reencoded.size()) {
            continue;  // Size mismatch - skip
        }
        
        // Replace in output file
        memcpy(&decrypted_file_data[nalu_payload_start], nalu_reencoded.data(), nalu_reencoded.size());
        
        decrypted_count++;
        
        if ((i + 1) % 5000 == 0) {
            cout << "  Processed " << (i + 1) << " NALUs...\n";
        }
    }

    ofstream output(output_file, ios::binary);
    output.write((char*)decrypted_file_data.data(), decrypted_file_data.size());
    output.close();

    cout << "\n✅ Decryption (S1) complete!\n";
    cout << "  📊 Total NALUs: " << nalu_positions.size() << "\n";
    cout << "  🔓 Decrypted: " << decrypted_count << "\n";
    cout << "  📄 Output file: " << output_file << " (" << decrypted_file_data.size() << " bytes)\n";
    
    return 0;
}
