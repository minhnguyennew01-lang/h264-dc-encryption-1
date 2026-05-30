#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include "arnold_2d.h"
#include "dc_metadata.h"

using namespace std;

/**
 * Extract DC components from H.264 NALU
 * DC offset: 20 bytes
 * DC size: 24 bytes (16Y + 4Cb + 4Cr)
 */
void extract_dc_from_nalu(
    const vector<uint8_t>& nalu,
    vector<uint8_t>& y_dc,
    vector<uint8_t>& cb_dc,
    vector<uint8_t>& cr_dc
) {
    const int DC_OFFSET = 20;
    const int Y_SIZE = 16;
    const int CB_SIZE = 4;
    const int CR_SIZE = 4;
    
    y_dc.clear();
    cb_dc.clear();
    cr_dc.clear();
    
    // Extract Y (16 bytes)
    for (int i = 0; i < Y_SIZE && DC_OFFSET + i < (int)nalu.size(); i++) {
        y_dc.push_back(nalu[DC_OFFSET + i]);
    }
    
    // Extract Cb (4 bytes)
    for (int i = 0; i < CB_SIZE && DC_OFFSET + Y_SIZE + i < (int)nalu.size(); i++) {
        cb_dc.push_back(nalu[DC_OFFSET + Y_SIZE + i]);
    }
    
    // Extract Cr (4 bytes)
    for (int i = 0; i < CR_SIZE && DC_OFFSET + Y_SIZE + CB_SIZE + i < (int)nalu.size(); i++) {
        cr_dc.push_back(nalu[DC_OFFSET + Y_SIZE + CB_SIZE + i]);
    }
}

/**
 * Reconstruct DC components back to NALU
 */
void reconstruct_dc_to_nalu(
    vector<uint8_t>& nalu,
    const vector<uint8_t>& y_dc,
    const vector<uint8_t>& cb_dc,
    const vector<uint8_t>& cr_dc
) {
    const int DC_OFFSET = 20;
    const int Y_SIZE = 16;
    
    // Reconstruct Y
    for (size_t i = 0; i < y_dc.size() && DC_OFFSET + i < nalu.size(); i++) {
        nalu[DC_OFFSET + i] = y_dc[i];
    }
    
    // Reconstruct Cb
    for (size_t i = 0; i < cb_dc.size() && DC_OFFSET + Y_SIZE + i < nalu.size(); i++) {
        nalu[DC_OFFSET + Y_SIZE + i] = cb_dc[i];
    }
    
    // Reconstruct Cr
    for (size_t i = 0; i < cr_dc.size() && DC_OFFSET + Y_SIZE + 4 + i < nalu.size(); i++) {
        nalu[DC_OFFSET + Y_SIZE + 4 + i] = cr_dc[i];
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <input.h264>\n";
        cerr << "Output: input.h264.encrypted (encrypted file)\n";
        cerr << "        input.h264.meta (metadata with padding)\n";
        return 1;
    }

    string input_file = argv[1];
    string output_file = input_file + ".encrypted";
    string metadata_file = input_file + ".meta";

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

    // Extract NALUs
    // Parse NALUs and track positions (optimized with memmem-like search)
    vector<pair<size_t, size_t>> nalu_positions;  // start, end positions
    size_t pos = 0;
    
    while (pos < file_data.size()) {
        // Find next 0x00 0x00 0x00 0x01 pattern
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
        
        // Find next start code
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

    // Encrypt DC components with Arnold 2D
    cout << "\n🔐 Encrypting DC with Arnold 2D 5×5 Cat Map...\n";
    
    vector<uint8_t> encrypted_file_data = file_data;
    vector<DCMetadataWithPadding> metadata_list;
    
    int encrypted_count = 0;
    int skipped_count = 0;
    
    for (size_t i = 0; i < nalu_positions.size(); i++) {
        size_t start = nalu_positions[i].first;
        size_t end = nalu_positions[i].second;
        vector<uint8_t> nalu(file_data.begin() + start, file_data.begin() + end);
        
        uint8_t nalu_type = nalu[4] & 0x1F;
        
        // Skip SPS (7) and PPS (8)
        if (nalu_type == 7 || nalu_type == 8) {
            skipped_count++;
            continue;
        }
        
        // Extract DC
        vector<uint8_t> y_dc, cb_dc, cr_dc;
        extract_dc_from_nalu(nalu, y_dc, cb_dc, cr_dc);
        
        // Skip if DC incomplete
        if (y_dc.size() < 16 || cb_dc.size() < 4 || cr_dc.size() < 4) {
            skipped_count++;
            continue;
        }
        
        // Arnold 2D Cat Map encryption (5×5 matrix with padding)
        uint8_t y_padding = 0x00;
        uint8_t cb_padding = 0x00;
        uint8_t cr_padding = 0x00;
        
        vector<uint8_t> y_encrypted = Arnold2D::encrypt_dc_arnold2d(y_dc, y_padding);
        vector<uint8_t> cb_encrypted = Arnold2D::encrypt_dc_arnold2d(cb_dc, cb_padding);
        vector<uint8_t> cr_encrypted = Arnold2D::encrypt_dc_arnold2d(cr_dc, cr_padding);
        
        // Reconstruct NALU with encrypted DC
        vector<uint8_t> nalu_encrypted = nalu;
        reconstruct_dc_to_nalu(nalu_encrypted, y_encrypted, cb_encrypted, cr_encrypted);
        
        // Update encrypted file data directly
        copy(nalu_encrypted.begin(), nalu_encrypted.end(), 
             encrypted_file_data.begin() + start);
        
        // Store metadata
        DCMetadataWithPadding meta;
        meta.nalu_index = i;
        meta.y_start = 0;      // Placeholder
        meta.y_count = 16;
        meta.cb_start = 16;    // Placeholder
        meta.cb_count = 4;
        meta.cr_start = 20;    // Placeholder
        meta.cr_count = 4;
        meta.y_padding = y_padding;
        meta.cb_padding = cb_padding;
        meta.cr_padding = cr_padding;
        
        metadata_list.push_back(meta);
        encrypted_count++;
        
        if ((encrypted_count + 1) % 5000 == 0) {
            cout << "  Encrypted " << encrypted_count << " NALUs...\n";
        }
    }

    // Write encrypted file
    ofstream output(output_file, ios::binary);
    output.write((char*)encrypted_file_data.data(), encrypted_file_data.size());
    output.close();

    // Save metadata
    save_dc_metadata_with_padding(metadata_file, metadata_list);

    // Summary
    cout << "\n✅ Encryption complete!\n";
    cout << "  • Total NALUs: " << nalu_positions.size() << "\n";
    cout << "  • Encrypted: " << encrypted_count << "\n";
    cout << "  • Skipped (SPS/PPS): " << skipped_count << "\n";
    cout << "  • Output file: " << output_file << " (" << encrypted_file_data.size() << " bytes)\n";
    cout << "  • Metadata file: " << metadata_file << "\n";
    
    return 0;
}
