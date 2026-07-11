#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <chrono>
#include <iomanip>
#include "../algorithms/encryption_interface.h"

using namespace std;

struct NALUInfo { uint32_t start_pos; uint32_t nal_pos; uint8_t type; };

static string get_basename(const string& path) {
    size_t pos = path.find_last_of("/\\");
    return (pos == string::npos) ? path : path.substr(pos + 1);
}

static string strip_algo_suffix(const string& encrypted_path) {
    string name = get_basename(encrypted_path);
    static const char* suffixes[] = {
        ".s2_hybrid_fixed", ".s2_aes", ".s2_des", ".s2_rc4", nullptr
    };
    for (int i = 0; suffixes[i]; i++) {
        string suf(suffixes[i]);
        if (name.size() > suf.size() &&
            name.substr(name.size() - suf.size()) == suf)
            return name.substr(0, name.size() - suf.size());
    }
    return name;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0]
             << " <encrypted_file> <key> [nalu_info.bin] [--algo hybrid|aes|des|rc4]\n";
        cerr << "  Default algo: hybrid\n";
        cerr << "  Default nalu_info: nalu_cache/<original_video>.nalu_info.bin\n";
        return 1;
    }

    string encrypted_file = argv[1];
    string key_str        = argv[2];
    string algo_name      = "hybrid";
    string nalu_info_file = "";

    for (int a = 3; a < argc; a++) {
        if (string(argv[a]) == "--algo" && a + 1 < argc) {
            algo_name = argv[++a];
        } else {
            nalu_info_file = argv[a];
        }
    }

    if (nalu_info_file.empty()) {
        string video_basename = strip_algo_suffix(encrypted_file);
        nalu_info_file = "nalu_cache/" + video_basename + ".nalu_info.bin";
    }

    string output_file = encrypted_file + ".decrypted";

    unique_ptr<IEncryption> algo;
    try { algo = IEncryption::create(algo_name); }
    catch (const exception& e) { cerr << "❌ " << e.what() << "\n"; return 1; }

    vector<uint8_t> key_bytes(key_str.begin(), key_str.end());

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
        nalu_in.read((char*)&nalus[i].nal_pos,   sizeof(nalus[i].nal_pos));
        nalu_in.read((char*)&nalus[i].type,       sizeof(nalus[i].type));
    }
    nalu_in.close();

    ifstream input(encrypted_file, ios::binary);
    if (!input) { cerr << "❌ Error: Cannot open " << encrypted_file << "\n"; return 1; }
    vector<uint8_t> file_data((istreambuf_iterator<char>(input)), istreambuf_iterator<char>());
    input.close();

    cout << "📄 Encrypted file: " << encrypted_file << " (" << file_data.size() << " bytes)\n";
    cout << "🔑 Key: " << key_str << "\n";
    cout << "🔐 Algorithm: " << algo_name << "\n";
    cout << "📊 Strategy: S2 (Type 5 only)\n";
    cout << "🔍 NALU entries: " << nalus.size() << "\n";

    vector<uint8_t> out_data = file_data;
    int decrypted_count = 0;
    long long dc_dec_ns = 0;
    vector<pair<int,string>> failed;

    const int DC_OFFSET = 19, DC_SIZE = 25;

    string enc_basename = get_basename(encrypted_file);
    string enc_dir = encrypted_file.substr(0, encrypted_file.size() - enc_basename.size());
    if (enc_dir.empty()) enc_dir = "./";
    string dc_txt_path = enc_dir + enc_basename + ".dc_dump_decrypted.txt";

    ofstream dc_txt(dc_txt_path);
    dc_txt << "# NALU DC dump (after decryption) - S2 - Algo: " << algo_name << "\n";
    dc_txt << "# Format: NALU <idx> [Type:<t>]: <25 hex bytes>\n";

    for (size_t idx = 0; idx < nalus.size(); idx++) {
        if (nalus[idx].type != 5) continue;

        size_t nalu_payload_start = nalus[idx].nal_pos;
        size_t nalu_payload_end   = file_data.size();
        if (idx + 1 < nalus.size()) nalu_payload_end = nalus[idx+1].start_pos;

        if (nalu_payload_end <= nalu_payload_start) {
            failed.push_back({(int)idx, "invalid_bounds"}); continue;
        }
        if (nalu_payload_end - nalu_payload_start < (size_t)(DC_OFFSET + DC_SIZE)) {
            failed.push_back({(int)idx, "too_small"}); continue;
        }

        vector<uint8_t> dc_encrypted(
            file_data.begin() + nalu_payload_start + DC_OFFSET,
            file_data.begin() + nalu_payload_start + DC_OFFSET + DC_SIZE);

        auto t0 = chrono::high_resolution_clock::now();
        vector<uint8_t> dc_decrypted = algo->decrypt(dc_encrypted, key_bytes, (int)idx);
        auto t1 = chrono::high_resolution_clock::now();
        dc_dec_ns += chrono::duration_cast<chrono::nanoseconds>(t1 - t0).count();

        if ((int)dc_decrypted.size() < DC_SIZE) {
            failed.push_back({(int)idx, "decrypt_too_small"}); continue;
        }
        memcpy(&out_data[nalu_payload_start + DC_OFFSET], dc_decrypted.data(), DC_SIZE);

        dc_txt << "NALU " << idx << " [Type:" << (int)nalus[idx].type << "]: ";
        for (int b = 0; b < DC_SIZE; b++)
            dc_txt << hex << uppercase << setw(2) << setfill('0') << (int)dc_decrypted[b] << " ";
        dc_txt << dec << "\n";

        decrypted_count++;
    }

    dc_txt.close();

    ofstream out(output_file, ios::binary);
    out.write((char*)out_data.data(), out_data.size());
    out.close();

    string failed_path = encrypted_file + ".failed_s2.txt";
    ofstream fo(failed_path);
    for (auto& p : failed) fo << p.first << ": " << p.second << "\n";
    fo.close();

    double dc_dec_ms      = dc_dec_ns / 1e6;
    long long ns_per_nalu = (decrypted_count > 0) ? dc_dec_ns / decrypted_count : 0;

    cout << "\n✅ Decryption complete! (" << algo_name << " S2)\n";
    cout << "  📊 Total NALUs: " << nalus.size() << "\n";
    cout << "  🔓 Decrypted: " << decrypted_count << "\n";
    cout << "  ❌ Failed/skipped: " << failed.size() << " (see " << failed_path << ")\n";
    cout << "  📄 Output: " << output_file << "\n";
    cout << "  📝 DC dump: " << dc_txt_path << "\n";
    cout << fixed << setprecision(3);
    cout << "  ⏱  DC_ONLY_TIME_MS: " << dc_dec_ms << "\n";
    cout << "  ⏱  DC_ONLY_NS_PER_NALU: " << ns_per_nalu << "\n";

    return 0;
}
