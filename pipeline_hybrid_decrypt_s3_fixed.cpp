#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <set>
#include "hybrid_encryption.h"
#include "dc_metadata.h"

using namespace std;

// Rigorously handle emulation prevention:
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
        cerr << "Usage: " << argv[0] << " <input_encrypted.h264> <key>\n";
        cerr << "Output: <input>.decrypted\n";
        return 1;
    }

    string input_file = argv[1];
    string key_str = argv[2];
    string output_file = input_file + ".decrypted";
    string meta_file = input_file + ".meta";
    
    vector<uint8_t> key_bytes(key_str.begin(), key_str.end());

    // Read encrypted file
    ifstream input(input_file, ios::binary);
    if (!input) {
        cerr << "❌ Error: Cannot open input file: " << input_file << "\n";
        return 1;
    }
    
    vector<uint8_t> file_data((istreambuf_iterator<char>(input)), 
                              istreambuf_iterator<char>());
    input.close();

    cout << "📄 Encrypted file: " << input_file << " (" << file_data.size() << " bytes)\n";
    cout << "🔑 Key: " << key_str << "\n";
    cout << "📊 Strategy: S3 HYBRID decrypt (mixed frequency)\n";

    // Read metadata
    set<int> encrypted_indices;
    ifstream meta(meta_file, ios::binary);
    if (meta) {
        char header[4];
        meta.read(header, 4);
        
        if (strncmp(header, "S3FX", 4) == 0) {
            uint32_t idx;
            while (meta.read((char*)&idx, 4)) {
                encrypted_indices.insert(idx);
            }
        }
        meta.close();
    }

    cout << "📋 Metadata: Found " << encrypted_indices.size() << " encrypted NALUs\n";

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

    vector<uint8_t> decrypted_file_data = file_data;
    int decrypted_count = 0;
    
    cout << "\n🔓 Decrypting...\n";
    
    // Decrypt NALUs
    for (size_t i = 0; i < nalu_positions.size(); i++) {
        if (encrypted_indices.find(i) == encrypted_indices.end()) {
            continue;  // Not encrypted
        }
        
        size_t nal_start_offset = nalu_positions[i].first;
        int nal_type = nalu_positions[i].second;
        
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
        
        // Extract NALU payload
        vector<uint8_t> nalu_raw(file_data.begin() + nalu_payload_start,
                                file_data.begin() + nalu_payload_end);
        
        // Remove emulation prevention
        vector<uint8_t> nalu_clean = NALUPayloadHandler::remove_emulation_prevention(nalu_raw);
        
        // Decrypt DC
        const int DC_OFFSET = 19;
        const int DC_SIZE = 25;
        
        if (nalu_clean.size() < DC_OFFSET + DC_SIZE) {
            continue;
        }
        
        // Extract encrypted DC
        vector<uint8_t> dc_encrypted(nalu_clean.begin() + DC_OFFSET,
                                     nalu_clean.begin() + DC_OFFSET + DC_SIZE);
        
        // Decrypt
        uint8_t padding_byte = 0;
        vector<uint8_t> dc_decrypted_full = HybridEncryption::decrypt_hybrid(
            dc_encrypted, key_bytes, (int)i, padding_byte);
        
        vector<uint8_t> dc_decrypted(dc_decrypted_full.begin(),
                                     dc_decrypted_full.begin() + DC_SIZE);
        
        // Replace in clean payload
        vector<uint8_t> nalu_clean_decrypted = nalu_clean;
        memcpy(&nalu_clean_decrypted[DC_OFFSET], dc_decrypted.data(), DC_SIZE);
        
        // Re-insert emulation prevention
        vector<uint8_t> nalu_reencoded = NALUPayloadHandler::insert_emulation_prevention(nalu_clean_decrypted);
        
        // Check size match
        if (nalu_payload_end - nalu_payload_start != nalu_reencoded.size()) {
            continue;
        }
        
        // Replace in output
        memcpy(&decrypted_file_data[nalu_payload_start], nalu_reencoded.data(), nalu_reencoded.size());
        
        decrypted_count++;
        
        if ((i + 1) % 5000 == 0) {
            cout << "  Decrypted " << (i + 1) << " NALUs...\n";
        }
    }

    // Write decrypted file
    ofstream output(output_file, ios::binary);
    output.write((char*)decrypted_file_data.data(), decrypted_file_data.size());
    output.close();

    cout << "\n✅ S3 Decryption complete!\n";
    cout << "  🔓 Decrypted: " << decrypted_count << " NALUs\n";
    cout << "  📄 Output file: " << output_file << " (" << decrypted_file_data.size() << " bytes)\n";
    
    return 0;
}
