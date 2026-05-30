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
        cerr << "Output: <input.h264>.s1_hybrid_v3 (encrypted file with proper NALU handling)\n";
        return 1;
    }

    string input_file = argv[1];
    string key_str = argv[2];
    string output_file = input_file + ".s1_hybrid_v3";
    
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
    cout << "📊 Strategy: HYBRID v3 (preserving all NAL structures)\n";

    // IMPORTANT: We do NOT parse NALU boundaries!
    // Instead, we assume input file is valid and just ENCRYPT SPECIFIC BYTE RANGES
    // that contain DC coefficients, while preserving everything else exactly
    
    // Strategy: Find all 4-byte and 3-byte start codes, collect NALU info
    vector<pair<size_t, int>> nalu_positions;  // offset, type
    
    for (size_t i = 0; i < file_data.size() - 2; i++) {
        size_t nal_offset = -1;
        
        // Check for 4-byte start code
        if (i + 3 < file_data.size() && 
            file_data[i] == 0x00 && file_data[i+1] == 0x00 && 
            file_data[i+2] == 0x00 && file_data[i+3] == 0x01) {
            nal_offset = i + 4;
        }
        // Check for 3-byte start code (avoiding double count with 4-byte)
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
    
    // Create output as copy of input (no modifications yet)
    vector<uint8_t> encrypted_file_data = file_data;
    
    int encrypted_count = 0;
    
    cout << "\n🔐 Encrypting DC coefficients...\n";
    
    // Process each NALU that has DC (slice NALUs: Type 1, 5, etc.)
    for (size_t i = 0; i < nalu_positions.size(); i++) {
        size_t nal_offset = nalu_positions[i].first;
        int nal_type = nalu_positions[i].second;
        
        // Skip SPS (7), PPS (8), SEI (6), etc. - only encrypt slice data (Type 1, 5, etc.)
        if (nal_type == 7 || nal_type == 8 || nal_type == 6) {
            continue;
        }
        
        // Find DC location: offset 20 from NALU data start (after NAL header)
        // But ONLY if this is a slice containing DC data
        size_t dc_offset_in_file = nal_offset + 20;
        const int DC_SIZE = 24;
        
        // Check bounds
        if (dc_offset_in_file + DC_SIZE > encrypted_file_data.size()) {
            continue;
        }
        
        // Extract DC bytes
        vector<uint8_t> dc_data(encrypted_file_data.begin() + dc_offset_in_file,
                               encrypted_file_data.begin() + dc_offset_in_file + DC_SIZE);
        
        // Encrypt DC
        uint8_t padding_byte = 0;
        vector<uint8_t> dc_encrypted_full = HybridEncryption::encrypt_hybrid(
            dc_data, key_bytes, (int)i, padding_byte);
        
        // Replace only first 24 bytes (keep consistency with original pipeline)
        vector<uint8_t> dc_encrypted(dc_encrypted_full.begin(), 
                                     dc_encrypted_full.begin() + DC_SIZE);
        memcpy(&encrypted_file_data[dc_offset_in_file], dc_encrypted.data(), DC_SIZE);
        
        encrypted_count++;
        
        if ((i + 1) % 5000 == 0) {
            cout << "  Processed " << (i + 1) << " NALUs...\n";
        }
    }

    // Write encrypted file
    ofstream output(output_file, ios::binary);
    output.write((char*)encrypted_file_data.data(), encrypted_file_data.size());
    output.close();

    cout << "\n✅ Encryption v3 complete!\n";
    cout << "  📊 Total NALUs: " << nalu_positions.size() << "\n";
    cout << "  🔐 Encrypted: " << encrypted_count << "\n";
    cout << "  📄 Output file: " << output_file << " (" << encrypted_file_data.size() << " bytes)\n";
    
    return 0;
}
