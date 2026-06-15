#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <cstring>
#include "hybrid_encryption.h"

using namespace std;

class NALUPayloadHandler {
public:
    static vector<uint8_t> remove_emulation_prevention(const vector<uint8_t>& payload) { return {};} 
    static vector<uint8_t> insert_emulation_prevention(const vector<uint8_t>& data) { return data; }
};

struct NALUInfo { uint32_t start_pos; uint32_t nal_pos; uint8_t type; };

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <encrypted.h264.s2_hybrid_fixed> <key> [nalu_info.bin]\n";
        return 1;
    }

    string encrypted_file = argv[1];
    string key_str = argv[2];
    string nalu_info_file = (argc >= 4) ? argv[3] : string("nalu_info.bin");
    string output_file = encrypted_file + ".decrypted";

    vector<uint8_t> key_bytes(key_str.begin(), key_str.end());

    ifstream nalu_in(nalu_info_file, ios::binary);
    if (!nalu_in) { cerr << "❌ Error: Cannot open " << nalu_info_file << "\n"; return 1; }
    uint32_t nalu_count = 0; nalu_in.read((char*)&nalu_count, sizeof(nalu_count));
    vector<NALUInfo> nalus(nalu_count);
    for (uint32_t i=0;i<nalu_count;i++){ nalu_in.read((char*)&nalus[i].start_pos,4); nalu_in.read((char*)&nalus[i].nal_pos,4); nalu_in.read((char*)&nalus[i].type,1);} nalu_in.close();

    ifstream input(encrypted_file, ios::binary); if(!input){ cerr<<"❌ Error: Cannot open encrypted file\n"; return 1;}    
    vector<uint8_t> file_data((istreambuf_iterator<char>(input)), istreambuf_iterator<char>());
    input.close();

    cout<<"📄 Encrypted file: "<<encrypted_file<<" ("<<file_data.size()<<" bytes)\n";
    cout<<"🔑 Key: "<<key_str<<"\n";
    cout<<"📊 Strategy: HYBRID S2 (h264_analyze)\n";
    cout<<"🔍 NALU entries: "<<nalus.size()<<"\n";

    vector<uint8_t> out_data = file_data; int decrypted_count=0; vector<pair<int,string>> failed;
    const int DC_OFFSET = 19; const int DC_SIZE = 25;

    for (size_t idx=0; idx<nalus.size(); idx++){
        if (nalus[idx].type != 5) continue; // S2: only Type 5 (I-frames)
        size_t nalu_payload_start = nalus[idx].nal_pos;
        size_t nalu_payload_end = file_data.size(); if (idx+1 < nalus.size()) nalu_payload_end = nalus[idx+1].start_pos;
        if (nalu_payload_end <= nalu_payload_start) { failed.push_back({(int)idx,"invalid_bounds"}); continue; }
        if (nalu_payload_end - nalu_payload_start < (size_t)(DC_OFFSET + DC_SIZE)) { failed.push_back({(int)idx,"too_small"}); continue; }

        vector<uint8_t> dc_encrypted(file_data.begin()+nalu_payload_start+DC_OFFSET, file_data.begin()+nalu_payload_start+DC_OFFSET+DC_SIZE);
        uint8_t padding_byte=0; auto dc_decrypted = HybridEncryption::decrypt_hybrid(dc_encrypted, key_bytes, (int)idx, padding_byte);
        if (dc_decrypted.size() < (size_t)DC_SIZE) { failed.push_back({(int)idx,"decrypt_small"}); continue; }
        memcpy(&out_data[nalu_payload_start+DC_OFFSET], dc_decrypted.data(), DC_SIZE);
        decrypted_count++;
    }

    ofstream out(output_file, ios::binary); out.write((char*)out_data.data(), out_data.size()); out.close();
    string failed_path = encrypted_file + ".failed_indices_s2.txt"; ofstream fo(failed_path.c_str()); for(auto &p:failed) fo<<p.first<<": "<<p.second<<"\n"; fo.close();

    cout<<"\n✅ Decryption (S2 h264_analyze) complete!\n";
    cout<<"  📊 Total NALUs: "<<nalus.size()<<"\n";
    cout<<"  🔓 Decrypted: "<<decrypted_count<<"\n";
    cout<<"  ❌ Failed/skipped: "<<failed.size()<<" (written to "<<failed_path<<")\n";
    cout<<"  📄 Output file: "<<output_file<<"\n";
    return 0;
}
