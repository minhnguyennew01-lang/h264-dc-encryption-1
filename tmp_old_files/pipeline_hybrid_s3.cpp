#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include "hybrid_encryption.h"
#include "dc_metadata.h"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <input.h264> <key>\n";
        cerr << "Output: <input.h264>.s3_hybrid (encrypted file)\n";
        cerr << "        <input.h264>.s3_hybrid.meta (metadata with padding)\n";
        return 1;
    }

    string input_file = argv[1];
    string key_str = argv[2];
    string output_file = input_file + ".s3_hybrid";
    string metadata_file = input_file + ".s3_hybrid.meta";
    
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
    cout << "📋 Mode: I-frames + every 3rd P/B frame - Strategy 3\n";

    // Extract NALUs
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

    // Encrypt DC components
    cout << "\n🔐 Encrypting DC with Hybrid (PLCM + Arnold 2D + XOR)...\n";
    
    vector<uint8_t> encrypted_file_data = file_data;
    vector<DCMetadataWithPadding> metadata_list;
    
    int encrypted_count = 0;
    int skipped_count = 0;
    int i_frame_count = 0;
    int p_b_encrypted_count = 0;
    int p_b_skipped_count = 0;
    int pb_frame_counter = 0;
    
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
        
        // Determine if I-frame or P/B frame
        bool is_i_frame = (nalu_type == 5 || nalu_type == 2);  // IDR slice or I-slice
        bool is_p_or_b = (nalu_type == 1) || (nalu_type == 3) || (nalu_type == 4);
        
        bool should_encrypt = false;
        
        if (is_i_frame) {
            should_encrypt = true;
            i_frame_count++;
        } else if (is_p_or_b) {
            pb_frame_counter++;
            // Encrypt every 3rd P/B frame
            if (pb_frame_counter % 3 == 0) {
                should_encrypt = true;
                p_b_encrypted_count++;
            } else {
                p_b_skipped_count++;
            }
        } else {
            continue;
        }
        
        if (!should_encrypt) continue;
        
        // Extract DC components (ALL 24 bytes as one unit)
        const int DC_OFFSET = 20;
        const int DC_TOTAL = 24;
        
        if (nalu_start + DC_OFFSET + DC_TOTAL > file_data.size()) {
            continue;
        }
        
        vector<uint8_t> dc_data(DC_TOTAL);
        memcpy(dc_data.data(), &file_data[nalu_start + DC_OFFSET], DC_TOTAL);
        
        // Encrypt ALL 24 bytes together with hybrid method
        // encrypt_hybrid returns 25 bytes encrypted, we store only 24
        uint8_t padding_byte = 0;
        vector<uint8_t> dc_encrypted_full = HybridEncryption::encrypt_hybrid(dc_data, key_bytes, (int)i, padding_byte);
        
        // Store only first 24 bytes in file (truncate the 25-byte encrypted result)
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
        
        if ((i + 1) % 1000 == 0) {
            cout << "  Processed " << (i + 1) << " NALUs...\n";
        }
    }

    // Write encrypted file
    ofstream output(output_file, ios::binary);
    output.write((char*)encrypted_file_data.data(), encrypted_file_data.size());
    output.close();

    // Save metadata
    save_dc_metadata_with_padding(metadata_file, metadata_list);

    // Summary
    cout << "\n✅ Strategy 3 Encryption complete!\n";
    cout << "  📊 Total NALUs: " << nalu_positions.size() << "\n";
    cout << "  🎬 I-frames encrypted: " << i_frame_count << "\n";
    cout << "  🎬 P/B frames encrypted (every 3rd): " << p_b_encrypted_count << "\n";
    cout << "  ⏭️  P/B frames skipped: " << p_b_skipped_count << "\n";
    cout << "  ⏭️  Metadata (SPS/PPS): " << skipped_count << "\n";
    cout << "  🔐 Total encrypted: " << encrypted_count << "\n";
    cout << "  📄 Output file: " << output_file << " (" << encrypted_file_data.size() << " bytes)\n";
    cout << "  📋 Metadata file: " << metadata_file << "\n";
    
    return 0;
}
