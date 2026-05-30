#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <ctime>
#include "hybrid_encryption.h"
#include "dc_metadata.h"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <input.h264> <key>\n";
        cerr << "Output: <input.h264>.s1_hybrid (encrypted file)\n";
        cerr << "        <input.h264>.s1_hybrid.meta (metadata with padding)\n";
        return 1;
    }

    string input_file = argv[1];
    string key_str = argv[2];
    string output_file = input_file + ".s1_hybrid";
    string metadata_file = input_file + ".s1_hybrid.meta";
    
    vector<uint8_t> key_bytes(key_str.begin(), key_str.end());

    // Read input file
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
    cout << "📊 Strategy: HYBRID (PLCM + Arnold 2D + Diffusion XOR)\n";
    cout << "📋 Mode: ALL frames (I, P, B) - Strategy 1\n";

    // Extract NALUs using optimized position tracking
    vector<pair<size_t, size_t>> nalu_positions;
    size_t pos = 0;
    
    while (pos < file_data.size()) {
        bool found = false;
        while (pos + 3 < file_data.size()) {
            if (file_data[pos] == 0x00 && file_data[pos+1] == 0x00 &&
                file_data[pos+2] == 0x00 && file_data[pos+3] == 0x01) {
                found = true;
                break;
            }
            pos++;
        }
        
        if (!found) break;
        
        size_t start = pos;
        pos += 4;
        size_t next = file_data.size();
        
        while (pos + 3 < file_data.size()) {
            if (file_data[pos] == 0x00 && file_data[pos+1] == 0x00 &&
                file_data[pos+2] == 0x00 && file_data[pos+3] == 0x01) {
                next = pos;
                break;
            }
            pos++;
        }
        
        nalu_positions.push_back({start, next});
        pos = next;
    }

    cout << "🔍 Found " << nalu_positions.size() << " NALUs\n";

    // Encrypt DC components with Hybrid Encryption
    cout << "\n🔐 Encrypting DC with Hybrid (PLCM + Arnold 2D + XOR)...\n";
    
    vector<uint8_t> encrypted_file_data = file_data;
    vector<DCMetadataWithPadding> metadata_list;
    
    int encrypted_count = 0;
    int skipped_count = 0;
    
    for (size_t i = 0; i < nalu_positions.size(); i++) {
        size_t nalu_start = nalu_positions[i].first;
        size_t nalu_end = nalu_positions[i].second;
        size_t nalu_size = nalu_end - nalu_start;
        
        if (nalu_size < 5) {
            skipped_count++;
            continue;
        }
        
        uint8_t nalu_type = file_data[nalu_start + 4] & 0x1F;
        
        // Skip SPS (7) and PPS (8)
        if (nalu_type == 7 || nalu_type == 8) {
            skipped_count++;
            continue;
        }
        
        // Extract DC components (offset 20, 24 bytes total: 16Y + 4Cb + 4Cr)
        const int DC_OFFSET = 20;
        const int Y_SIZE = 16;
        const int DC_TOTAL = 24;
        
        if (nalu_start + DC_OFFSET + DC_TOTAL > file_data.size()) {
            skipped_count++;
            continue;
        }
        
        // Extract ALL 24 DC bytes as one unit (16Y + 4Cb + 4Cr)
        vector<uint8_t> dc_data(DC_TOTAL);
        memcpy(dc_data.data(), &file_data[nalu_start + DC_OFFSET], DC_TOTAL);
        
        // Encrypt ALL 24 bytes together with hybrid method
        // encrypt_hybrid returns 25 bytes encrypted, we store only 24
        uint8_t padding_byte = 0;
        vector<uint8_t> dc_encrypted_full = HybridEncryption::encrypt_hybrid(dc_data, key_bytes, (int)i, padding_byte);
        
        // Store only first 24 bytes in file (truncate the 25-byte encrypted result)
        // The padding_byte tells decrypt how to reconstruct the 25-byte state
        vector<uint8_t> dc_encrypted(dc_encrypted_full.begin(), dc_encrypted_full.begin() + DC_TOTAL);
        memcpy(&encrypted_file_data[nalu_start + DC_OFFSET], dc_encrypted.data(), DC_TOTAL);
        
        // Store metadata with padding information
        DCMetadataWithPadding meta;
        meta.nalu_index = (int)i;
        meta.y_start = DC_OFFSET;
        meta.y_count = 16;
        meta.cb_start = DC_OFFSET + 16;
        meta.cb_count = 4;
        meta.cr_start = DC_OFFSET + 16 + 4;
        meta.cr_count = 4;
        meta.y_padding = padding_byte;
        meta.cb_padding = padding_byte;
        meta.cr_padding = padding_byte;
        meta.extra_byte_encrypted = dc_encrypted_full[24];  // Store the 25th encrypted byte!
        
        metadata_list.push_back(meta);
        encrypted_count++;
        
        if ((i + 1) % 5000 == 0) {
            cout << "  Encrypted " << (i + 1) << " NALUs...\n";
        }
    }

    // Write encrypted file
    ofstream output(output_file, ios::binary);
    output.write((char*)encrypted_file_data.data(), encrypted_file_data.size());
    output.close();

    // Save metadata
    save_dc_metadata_with_padding(metadata_file, metadata_list);

    // Summary
    cout << "\n✅ Strategy 1 Encryption complete!\n";
    cout << "  📊 Total NALUs: " << nalu_positions.size() << "\n";
    cout << "  🔐 Encrypted: " << encrypted_count << "\n";
    cout << "  ⏭️  Skipped (SPS/PPS): " << skipped_count << "\n";
    cout << "  📄 Output file: " << output_file << " (" << encrypted_file_data.size() << " bytes)\n";
    cout << "  📋 Metadata file: " << metadata_file << "\n";
    
    return 0;
}
