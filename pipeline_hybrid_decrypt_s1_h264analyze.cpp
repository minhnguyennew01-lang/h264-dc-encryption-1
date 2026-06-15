#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <cstring>
#include "hybrid_encryption.h"

using namespace std;

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
            if (i + 1 < data.size() && result.size() >= 2 &&
                result[result.size()-2] == 0x00 && result[result.size()-1] == 0x00) {
                uint8_t next_byte = data[i+1];
                if (next_byte <= 0x03) {
                    result.push_back(0x03);
                }
            }
        }
        return result;
    }
};

struct NALUInfo { uint32_t start_pos; uint32_t nal_pos; uint8_t type; };

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <encrypted.h264.s1_hybrid_fixed> <key> [nalu_info.bin]\n";
        return 1;
    }

    string encrypted_file = argv[1];
    string key_str = argv[2];
    string nalu_info_file = (argc >= 4) ? argv[3] : string("nalu_info.bin");
    string output_file = encrypted_file + ".decrypted";

    vector<uint8_t> key_bytes(key_str.begin(), key_str.end());

    // Load nalu_info.bin
    ifstream nalu_in(nalu_info_file, ios::binary);
    if (!nalu_in) {
        cerr << "❌ Error: Cannot open " << nalu_info_file << "\n";
        return 1;
    }
    uint32_t nalu_count = 0;
    nalu_in.read((char*)&nalu_count, sizeof(nalu_count));
    vector<NALUInfo> nalus(nalu_count);
    for (uint32_t i = 0; i < nalu_count; i++) {
        nalu_in.read((char*)&nalus[i].start_pos, sizeof(nalus[i].start_pos));
        nalu_in.read((char*)&nalus[i].nal_pos, sizeof(nalus[i].nal_pos));
        nalu_in.read((char*)&nalus[i].type, sizeof(nalus[i].type));
    }
    nalu_in.close();

    ifstream input(encrypted_file, ios::binary);
    if (!input) {
        cerr << "❌ Error: Cannot open encrypted file: " << encrypted_file << "\n";
        return 1;
    }
    vector<uint8_t> file_data((istreambuf_iterator<char>(input)), istreambuf_iterator<char>());
    input.close();

    cout << "📄 Encrypted file: " << encrypted_file << " (" << file_data.size() << " bytes)\n";
    cout << "🔑 Key: " << key_str << "\n";
    cout << "📊 Strategy: HYBRID S1 (h264_analyze)\n";
    cout << "🔍 NALU entries: " << nalus.size() << " (from " << nalu_info_file << ")\n";

    vector<uint8_t> out_data = file_data;
    int decrypted_count = 0;
    // Record failed indices and reasons for debugging
    vector<pair<int,string>> failed;

    const int DC_OFFSET = 19; // matches pipeline_hybrid_s1_h264analyze
    const int DC_SIZE = 25;   // encrypted block size (full 25 bytes)

    cout << "\n🔐 Processing decryption using authoritative NALU list...\n";

    for (size_t idx = 0; idx < nalus.size(); idx++) {
        int nal_type = nalus[idx].type;
        if (nal_type != 1 && nal_type != 5) continue; // S1 targets Type 1 and 5

        size_t nalu_payload_start = nalus[idx].nal_pos;
        size_t nalu_payload_end = file_data.size();
        if (idx + 1 < nalus.size()) nalu_payload_end = nalus[idx+1].start_pos;

        if (nalu_payload_end <= nalu_payload_start) {
            failed.push_back({(int)idx, "invalid_payload_bounds"});
            continue;
        }

        size_t payload_len = nalu_payload_end - nalu_payload_start;
        if (payload_len < (size_t)(DC_OFFSET + DC_SIZE)) {
            failed.push_back({(int)idx, "too_small_for_dc(raw)"});
            continue;
        }

        // Extract encrypted DC directly from raw file bytes (no emulation-prevention handling)
        vector<uint8_t> dc_encrypted(file_data.begin() + nalu_payload_start + DC_OFFSET,
                                     file_data.begin() + nalu_payload_start + DC_OFFSET + DC_SIZE);

        uint8_t padding_byte = 0; // we don't have stored padding; decrypt will return 25 bytes
        vector<uint8_t> dc_decrypted = HybridEncryption::decrypt_hybrid(dc_encrypted, key_bytes, (int)idx, padding_byte);

        if (dc_decrypted.size() < (size_t)DC_SIZE) {
            failed.push_back({(int)idx, "decrypt_hybrid_output_too_small"});
            continue;
        }

        // Write decrypted bytes directly back into the raw file region (in-place)
        memcpy(&out_data[nalu_payload_start + DC_OFFSET], dc_decrypted.data(), DC_SIZE);
        decrypted_count++;
    }

    ofstream out(output_file, ios::binary);
    out.write((char*)out_data.data(), out_data.size());
    out.close();

    // Write failed indices to file for debugging
    string failed_path = encrypted_file + ".failed_indices_s1.txt";
    ofstream failed_out(failed_path.c_str());
    for (auto &p : failed) {
        failed_out << p.first << ": " << p.second << "\n";
    }
    failed_out.close();

    cout << "\n✅ Decryption (S1 h264_analyze) complete!\n";
    cout << "  📊 Total NALUs (from nalu_info): " << nalus.size() << "\n";
    cout << "  🔓 Decrypted: " << decrypted_count << "\n";
    cout << "  ❌ Failed/skipped: " << failed.size() << " (written to " << failed_path << ")\n";
    cout << "  📄 Output file: " << output_file << " (" << out_data.size() << " bytes)\n";

    return 0;
}
