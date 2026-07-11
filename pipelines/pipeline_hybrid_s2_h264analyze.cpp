#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include "../algorithms/encryption_interface.h"

using namespace std;

struct NALUInfo { uint32_t start_pos; uint32_t nal_pos; uint8_t type; };

static string get_basename(const string& path) {
    size_t pos = path.find_last_of("/\\");
    return (pos == string::npos) ? path : path.substr(pos + 1);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <input.h264> <key> [--algo hybrid|aes|des|rc4]\n";
        cerr << "  Default algo: hybrid\n";
        cerr << "  Requires: nalu_cache/<video>.nalu_info.bin (run extract tool first)\n";
        return 1;
    }

    string input_file = argv[1];
    string key_str    = argv[2];
    string algo_name  = "hybrid";

    for (int a = 3; a < argc; a++) {
        if (string(argv[a]) == "--algo" && a + 1 < argc) algo_name = argv[++a];
    }

    unique_ptr<IEncryption> algo;
    try { algo = IEncryption::create(algo_name); }
    catch (const exception& e) { cerr << "❌ " << e.what() << "\n"; return 1; }

    string video_basename  = get_basename(input_file);
    string nalu_info_file  = "nalu_cache/" + video_basename + ".nalu_info.bin";
    string results_dir     = "results/" + video_basename + "/" + algo_name + "_s2";
    string suffix          = (algo_name == "hybrid") ? ".s2_hybrid_fixed" : ".s2_" + algo_name;
    string output_file     = results_dir + "/" + video_basename + suffix;
    string meta_file       = output_file + ".meta";
    string dc_txt_path     = results_dir + "/" + video_basename + ".dc_dump.txt";

    std::filesystem::create_directories(results_dir);

    vector<uint8_t> key_bytes(key_str.begin(), key_str.end());

    ifstream nalu_in(nalu_info_file, ios::binary);
    if (!nalu_in) {
        cerr << "❌ Error: Cannot open " << nalu_info_file << "\n";
        cerr << "   Run: ./extract/extract_nalu_from_h264analyze_wrapper.sh " << input_file << "\n";
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

    ifstream input(input_file, ios::binary);
    if (!input) { cerr << "❌ Error: Cannot open " << input_file << "\n"; return 1; }
    vector<uint8_t> file_data((istreambuf_iterator<char>(input)), istreambuf_iterator<char>());
    input.close();

    cout << "📄 Input file: " << input_file << " (" << file_data.size() << " bytes)\n";
    cout << "🔑 Key: " << key_str << "\n";
    cout << "🔐 Algorithm: " << algo_name << "\n";
    cout << "📊 Strategy: S2 (Type 5 I-frames only)\n";
    cout << "🔍 NALU entries: " << nalu_count << "\n";

    vector<uint8_t> encrypted_file_data = file_data;
    int encrypted_count = 0;
    long long dc_enc_ns = 0;

    ofstream dc_txt(dc_txt_path);
    dc_txt << "# NALU DC dump (before encryption) - S2 - Algo: " << algo_name << "\n";
    dc_txt << "# Format: NALU <idx> [Type:<t>]: <25 hex bytes>\n";

    cout << "\n🔐 Processing NALUs...\n";

    const int DC_OFFSET = 19, DC_SIZE = 25;

    for (size_t i = 0; i < nalus.size(); i++) {
        int nal_type = nalus[i].type;
        if (nal_type != 5) continue;

        size_t nalu_payload_start = nalus[i].nal_pos;
        size_t nalu_payload_end   = file_data.size();
        if (i + 1 < nalus.size()) nalu_payload_end = nalus[i+1].start_pos;

        if (nalu_payload_end <= nalu_payload_start) continue;
        if (nalu_payload_end - nalu_payload_start < (size_t)(DC_OFFSET + DC_SIZE)) continue;

        vector<uint8_t> dc_data(
            file_data.begin() + nalu_payload_start + DC_OFFSET,
            file_data.begin() + nalu_payload_start + DC_OFFSET + DC_SIZE);

        dc_txt << "NALU " << i << " [Type:" << nal_type << "]: ";
        for (int b = 0; b < DC_SIZE; b++)
            dc_txt << hex << uppercase << setw(2) << setfill('0') << (int)dc_data[b] << " ";
        dc_txt << dec << "\n";

        auto t0 = chrono::high_resolution_clock::now();
        vector<uint8_t> encrypted_dc = algo->encrypt(dc_data, key_bytes, (int)i);
        auto t1 = chrono::high_resolution_clock::now();
        dc_enc_ns += chrono::duration_cast<chrono::nanoseconds>(t1 - t0).count();

        if ((int)encrypted_dc.size() < DC_SIZE) {
            cerr << "⚠️  NALU " << i << ": encrypt returned < " << DC_SIZE << " bytes\n";
            continue;
        }
        for (int j = 0; j < DC_SIZE; j++)
            encrypted_file_data[nalu_payload_start + DC_OFFSET + j] = encrypted_dc[j];

        encrypted_count++;
    }

    dc_txt.close();

    ofstream output(output_file, ios::binary);
    output.write((char*)encrypted_file_data.data(), encrypted_file_data.size());
    output.close();

    ofstream meta(meta_file);
    meta << "Strategy: S2 (Type5 only)\n";
    meta << "Algorithm: " << algo_name << "\n";
    meta << "Total NALUs: " << nalu_count << "\n";
    meta << "Encrypted NALUs: " << encrypted_count << "\n";
    meta.close();

    double dc_enc_ms      = dc_enc_ns / 1e6;
    long long ns_per_nalu = (encrypted_count > 0) ? dc_enc_ns / encrypted_count : 0;

    cout << "\n✅ Encryption complete! (" << algo_name << " S2)\n";
    cout << "  📊 Total NALUs: " << nalu_count << "\n";
    cout << "  🔐 Encrypted (Type 5 only): " << encrypted_count << "\n";
    cout << "  📄 Output: " << output_file << "\n";
    cout << "  📝 DC dump: " << dc_txt_path << "\n";
    cout << fixed << setprecision(3);
    cout << "  ⏱  DC_ONLY_TIME_MS: " << dc_enc_ms << "\n";
    cout << "  ⏱  DC_ONLY_NS_PER_NALU: " << ns_per_nalu << "\n";

    return 0;
}
