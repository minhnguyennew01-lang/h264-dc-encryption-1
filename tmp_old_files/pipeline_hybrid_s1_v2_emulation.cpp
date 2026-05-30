#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <ctime>
#include "hybrid_encryption.h"
#include "dc_metadata.h"

using namespace std;

// Helper: Find actual NALU positions accounting for emulation prevention
struct NALUInfo {
    size_t raw_start;      // Position of start code in raw file
    size_t raw_end;        // Position of next start code (or end of file)
    size_t nal_header_pos; // Position of NAL header byte (after start code)
    int nal_type;          // NAL unit type
};

vector<NALUInfo> extract_nalus_with_emulation_handling(const vector<uint8_t>& data) {
    vector<NALUInfo> nalus;
    size_t pos = 0;
    
    while (pos < data.size()) {
        // Find next start code
        while (pos + 3 < data.size()) {
            if (data[pos] == 0x00 && data[pos+1] == 0x00 && 
                data[pos+2] == 0x00 && data[pos+3] == 0x01) {
                break;
            }
            pos++;
        }
        
        if (pos + 3 >= data.size()) break;
        
        size_t start_code_pos = pos;
        size_t nal_header_pos = pos + 4;
        
        if (nal_header_pos >= data.size()) break;
        
        uint8_t nal_byte = data[nal_header_pos];
        int nal_type = nal_byte & 0x1F;
        
        pos = nal_header_pos + 1;
        
        // Find next start code
        size_t next_start = data.size();
        while (pos + 3 < data.size()) {
            if (data[pos] == 0x00 && data[pos+1] == 0x00 && 
                data[pos+2] == 0x00 && data[pos+3] == 0x01) {
                next_start = pos;
                break;
            }
            pos++;
        }
        
        NALUInfo info;
        info.raw_start = start_code_pos;
        info.raw_end = next_start;
        info.nal_header_pos = nal_header_pos;
        info.nal_type = nal_type;
        
        nalus.push_back(info);
        pos = next_start;
    }
    
    return nalus;
}

// Remove emulation prevention bytes from NALU payload
vector<uint8_t> remove_emulation_prevention(const vector<uint8_t>& nalu_data) {
    vector<uint8_t> cleaned;
    
    for (size_t i = 0; i < nalu_data.size(); ) {
        if (i + 2 < nalu_data.size() && 
            nalu_data[i] == 0x00 && nalu_data[i+1] == 0x00 && 
            nalu_data[i+2] == 0x03) {
            // Found emulation prevention: skip the 0x03 byte
            cleaned.push_back(0x00);
            cleaned.push_back(0x00);
            i += 3;
        } else {
            cleaned.push_back(nalu_data[i]);
            i++;
        }
    }
    
    return cleaned;
}

// Insert emulation prevention where needed
vector<uint8_t> insert_emulation_prevention(const vector<uint8_t>& data) {
    vector<uint8_t> result;
    
    for (size_t i = 0; i < data.size(); i++) {
        result.push_back(data[i]);
        
        // Check if we need to insert emulation prevention byte
        if (i + 1 < data.size() && 
            data[i] == 0x00 && data[i+1] == 0x00) {
            // We need to check next byte
            if (i + 2 < data.size()) {
                uint8_t next = data[i+2];
                if (next == 0x00 || next == 0x01 || next == 0x02 || next == 0x03) {
                    result.push_back(0x03);  // Insert emulation prevention byte
                }
            }
        }
    }
    
    return result;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <input.h264> <key>\n";
        cerr << "Output: <input.h264>.s1_hybrid_v2 (encrypted file)\n";
        return 1;
    }

    string input_file = argv[1];
    string key_str = argv[2];
    string output_file = input_file + ".s1_hybrid_v2";
    
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
    cout << "📊 Strategy: HYBRID v2 (with emulation prevention handling)\n";

    // Extract NALUs properly
    vector<NALUInfo> nalus = extract_nalus_with_emulation_handling(file_data);
    cout << "🔍 Found " << nalus.size() << " NALUs\n";

    vector<uint8_t> encrypted_file_data;
    int encrypted_count = 0;
    int skipped_count = 0;
    
    cout << "\n📋 Processing NALUs:\n";
    for (size_t i = 0; i < nalus.size(); i++) {
        const NALUInfo& nalu = nalus[i];
        int nal_type = nalu.nal_type;
        
        printf("  NALU %3lu | Type %d | Raw [0x%08lx, 0x%08lx) | Size %lu bytes\n",
               i+1, nal_type, nalu.raw_start, nalu.raw_end, 
               nalu.raw_end - nalu.raw_start);
        
        // Copy start code
        encrypted_file_data.insert(encrypted_file_data.end(),
                                  file_data.begin() + nalu.raw_start,
                                  file_data.begin() + nalu.raw_start + 4);
        
        // Copy NAL header
        encrypted_file_data.push_back(file_data[nalu.nal_header_pos]);
        
        // Extract and process NALU payload (remove emulation prevention)
        size_t payload_start = nalu.nal_header_pos + 1;
        size_t payload_end = nalu.raw_end;
        vector<uint8_t> nalu_payload(file_data.begin() + payload_start,
                                     file_data.begin() + payload_end);
        
        vector<uint8_t> cleaned_payload = remove_emulation_prevention(nalu_payload);
        
        // Skip SPS (7) and PPS (8)
        if (nal_type == 7 || nal_type == 8) {
            // Just copy cleaned payload back with emulation prevention
            vector<uint8_t> reencoded = insert_emulation_prevention(cleaned_payload);
            encrypted_file_data.insert(encrypted_file_data.end(),
                                      reencoded.begin(), reencoded.end());
            skipped_count++;
            continue;
        }
        
        // Extract DC (offset 20 in cleaned payload, 24 bytes)
        const int DC_OFFSET = 20;
        const int DC_SIZE = 24;
        
        if (cleaned_payload.size() < DC_OFFSET + DC_SIZE) {
            printf("    ⚠️  Payload too small (%lu < %d). Skipping.\n", 
                   cleaned_payload.size(), DC_OFFSET + DC_SIZE);
            
            vector<uint8_t> reencoded = insert_emulation_prevention(cleaned_payload);
            encrypted_file_data.insert(encrypted_file_data.end(),
                                      reencoded.begin(), reencoded.end());
            skipped_count++;
            continue;
        }
        
        // Extract DC bytes
        vector<uint8_t> dc_data(cleaned_payload.begin() + DC_OFFSET,
                               cleaned_payload.begin() + DC_OFFSET + DC_SIZE);
        
        // Encrypt DC
        uint8_t padding_byte = 0;
        vector<uint8_t> dc_encrypted_full = HybridEncryption::encrypt_hybrid(dc_data, key_bytes, (int)i, padding_byte);
        vector<uint8_t> dc_encrypted(dc_encrypted_full.begin(), dc_encrypted_full.begin() + DC_SIZE);
        
        // Copy payload with encrypted DC
        vector<uint8_t> modified_payload = cleaned_payload;
        memcpy(&modified_payload[DC_OFFSET], dc_encrypted.data(), DC_SIZE);
        
        // Re-insert emulation prevention and append
        vector<uint8_t> reencoded = insert_emulation_prevention(modified_payload);
        encrypted_file_data.insert(encrypted_file_data.end(),
                                  reencoded.begin(), reencoded.end());
        
        encrypted_count++;
        
        if ((i + 1) % 1000 == 0) {
            cout << "  ... processed " << (i + 1) << " NALUs\n";
        }
    }

    // Write encrypted file
    ofstream output(output_file, ios::binary);
    output.write((char*)encrypted_file_data.data(), encrypted_file_data.size());
    output.close();

    cout << "\n✅ Encryption (v2) complete!\n";
    cout << "  📊 Total NALUs: " << nalus.size() << "\n";
    cout << "  🔐 Encrypted: " << encrypted_count << "\n";
    cout << "  ⏭️  Skipped: " << skipped_count << "\n";
    cout << "  📄 Output file: " << output_file << " (" << encrypted_file_data.size() << " bytes)\n";
    
    return 0;
}
