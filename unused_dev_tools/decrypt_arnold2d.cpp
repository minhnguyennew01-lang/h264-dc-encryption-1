#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include "arnold_2d.h"
#include "dc_metadata.h"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <input.h264.encrypted>\n";
        cerr << "Requires: input.h264.meta (metadata with padding)\n";
        cerr << "Output: input.h264.decrypted\n";
        return 1;
    }

    string encrypted_file = argv[1];
    
    // Derive metadata filename by removing .encrypted
    string metadata_file = encrypted_file;
    if (metadata_file.find(".encrypted") != string::npos) {
        metadata_file.erase(metadata_file.find(".encrypted"), 10);
    }
    metadata_file += ".meta";
    
    // Derive decrypted filename by removing .encrypted
    string output_file = encrypted_file;
    if (output_file.find(".encrypted") != string::npos) {
        output_file.erase(output_file.find(".encrypted"), 10);
    }
    output_file += ".decrypted";

    // Read encrypted file
    ifstream encrypted_input(encrypted_file, ios::binary);
    if (!encrypted_input) {
        cerr << "❌ Error: Cannot open encrypted file: " << encrypted_file << "\n";
        return 1;
    }
    
    vector<uint8_t> encrypted_data((istreambuf_iterator<char>(encrypted_input)), 
                                   istreambuf_iterator<char>());
    encrypted_input.close();

    cout << "📄 Encrypted file: " << encrypted_file << " (" << encrypted_data.size() << " bytes)\n";

    // Load metadata
    auto metadata_list = load_dc_metadata_with_padding(metadata_file);
    if (metadata_list.empty()) {
        cerr << "❌ Error: Failed to load metadata\n";
        return 1;
    }

    // Extract NALUs (optimized position tracking, no data copy)
    vector<pair<size_t, size_t>> nalu_positions;  // start, end positions
    size_t pos = 0;
    
    while (pos < encrypted_data.size()) {
        // Find next 0x00 0x00 0x00 0x01 pattern
        bool found = false;
        while (pos + 3 < encrypted_data.size()) {
            if (encrypted_data[pos] == 0x00 && encrypted_data[pos+1] == 0x00 &&
                encrypted_data[pos+2] == 0x00 && encrypted_data[pos+3] == 0x01) {
                found = true;
                break;
            }
            pos++;
        }
        
        if (!found) break;
        
        size_t start = pos;
        pos += 4;
        size_t next = encrypted_data.size();
        
        // Find next start code
        while (pos + 3 < encrypted_data.size()) {
            if (encrypted_data[pos] == 0x00 && encrypted_data[pos+1] == 0x00 &&
                encrypted_data[pos+2] == 0x00 && encrypted_data[pos+3] == 0x01) {
                next = pos;
                break;
            }
            pos++;
        }
        
        nalu_positions.push_back({start, next});
        pos = next;
    }

    cout << "🔍 Found " << nalu_positions.size() << " NALUs\n";

    // Decrypt DC components directly in file data using position tracking
    cout << "\n🔓 Decrypting DC with Arnold 2D 5×5 Cat Map...\n";
    
    vector<uint8_t> decrypted_file_data = encrypted_data;
    
    int decrypted_count = 0;
    int skipped_count = 0;
    int meta_idx = 0;
    
    for (size_t i = 0; i < nalu_positions.size(); i++) {
        size_t nalu_start = nalu_positions[i].first;
        size_t nalu_end = nalu_positions[i].second;
        size_t nalu_size = nalu_end - nalu_start;
        
        // Skip if NALU too small to extract type
        if (nalu_size < 5) {
            skipped_count++;
            continue;
        }
        
        uint8_t nalu_type = encrypted_data[nalu_start + 4] & 0x1F;
        
        // Skip SPS (7) and PPS (8)
        if (nalu_type == 7 || nalu_type == 8) {
            skipped_count++;
            continue;
        }
        
        if (meta_idx >= (int)metadata_list.size()) {
            cerr << "❌ Error: Not enough metadata entries\n";
            break;
        }
        
        const auto& meta = metadata_list[meta_idx];
        
        // Extract encrypted DC directly from file data
        const int DC_OFFSET = 20;
        const int Y_SIZE = 16;
        
        vector<uint8_t> y_encrypted(Y_SIZE);
        vector<uint8_t> cb_encrypted(4);
        vector<uint8_t> cr_encrypted(4);
        
        if (nalu_start + DC_OFFSET + 24 <= encrypted_data.size()) {
            memcpy(y_encrypted.data(), &encrypted_data[nalu_start + DC_OFFSET], Y_SIZE);
            memcpy(cb_encrypted.data(), &encrypted_data[nalu_start + DC_OFFSET + Y_SIZE], 4);
            memcpy(cr_encrypted.data(), &encrypted_data[nalu_start + DC_OFFSET + Y_SIZE + 4], 4);
        } else {
            skipped_count++;
            meta_idx++;
            continue;
        }
        
        // Decrypt with Arnold 2D inverse
        auto y_decrypted = Arnold2D::decrypt_dc_arnold2d(y_encrypted, meta.y_padding);
        auto cb_decrypted = Arnold2D::decrypt_dc_arnold2d(cb_encrypted, meta.cb_padding);
        auto cr_decrypted = Arnold2D::decrypt_dc_arnold2d(cr_encrypted, meta.cr_padding);
        
        // Update file data directly (in-place decryption)
        memcpy(&decrypted_file_data[nalu_start + DC_OFFSET], y_decrypted.data(), Y_SIZE);
        memcpy(&decrypted_file_data[nalu_start + DC_OFFSET + Y_SIZE], cb_decrypted.data(), 4);
        memcpy(&decrypted_file_data[nalu_start + DC_OFFSET + Y_SIZE + 4], cr_decrypted.data(), 4);
        
        decrypted_count++;
        meta_idx++;
        
        if ((i + 1) % 5000 == 0) {
            cout << "  Decrypted " << (i + 1) << " NALUs...\n";
        }
    }

    // Write decrypted file
    ofstream output(output_file, ios::binary);
    output.write((char*)decrypted_file_data.data(), decrypted_file_data.size());
    output.close();

    // Summary
    cout << "\n✅ Decryption complete!\n";
    cout << "  • Total NALUs: " << nalu_positions.size() << "\n";
    cout << "  • Decrypted: " << decrypted_count << "\n";
    cout << "  • Skipped (SPS/PPS): " << skipped_count << "\n";
    cout << "  • Output file: " << output_file << " (" << decrypted_file_data.size() << " bytes)\n";
    
    return 0;
}
