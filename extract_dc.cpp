#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <sstream>

using namespace std;

struct DCData {
    uint32_t nalu_index;
    vector<int> dc_coeffs;  // 24 DC coefficients: 16 LUMA + 4 CB + 4 CR
};

// Read DC values from metadata file (format: INDEX|DC0 DC1 DC2 ... DC23)
vector<DCData> read_dc_from_metadata(const string& filename) {
    ifstream file(filename);
    if (!file) {
        cerr << "Error: Cannot open DC metadata file: " << filename << "\n";
        return {};
    }

    vector<DCData> dc_data;
    string line;

    while (getline(file, line)) {
        if (line.empty()) continue;

        // Parse: INDEX|DC0 DC1 DC2 ... DC23
        size_t pipe_pos = line.find('|');
        if (pipe_pos == string::npos) continue;

        uint32_t index = stoul(line.substr(0, pipe_pos));
        string dc_str = line.substr(pipe_pos + 1);

        vector<int> dc_coeffs;
        stringstream ss(dc_str);
        int val;
        while (ss >> val) {
            dc_coeffs.push_back(val);
        }

        // Ensure exactly 24 DC coefficients
        while (dc_coeffs.size() < 24) {
            dc_coeffs.push_back(0);
        }
        if (dc_coeffs.size() > 24) {
            dc_coeffs.resize(24);
        }

        DCData data;
        data.nalu_index = index;
        data.dc_coeffs = dc_coeffs;
        dc_data.push_back(data);
    }

    file.close();
    return dc_data;
}

// Print DC coefficients to file
void print_dc_coefficients(const vector<DCData>& dc_data, const string& output_file) {
    ofstream out(output_file);
    if (!out) {
        cerr << "Error: Cannot create output file: " << output_file << "\n";
        return;
    }

    out << "╔═══════════════════════════════════════════════════════════════════╗\n";
    out << "║                    DC COEFFICIENTS ANALYSIS                       ║\n";
    out << "╚═══════════════════════════════════════════════════════════════════╝\n\n";

    out << "Total NALUs: " << dc_data.size() << "\n";
    out << "DC per NALU: 24 (16 LUMA + 4 CB + 4 CR)\n\n";

    // Print first 100 NALUs in detail
    int print_count = min(100, (int)dc_data.size());

    for (int i = 0; i < print_count; i++) {
        const auto& data = dc_data[i];
        out << "═══════════════════════════════════════════════════════════════════\n";
        out << "NALU #" << data.nalu_index << "\n";
        out << "───────────────────────────────────────────────────────────────────\n";

        out << "LUMA DC (16): ";
        for (int j = 0; j < 16; j++) {
            out << setw(4) << data.dc_coeffs[j] << " ";
        }
        out << "\n";

        out << "CB DC (4):   ";
        for (int j = 16; j < 20; j++) {
            out << setw(4) << data.dc_coeffs[j] << " ";
        }
        out << "\n";

        out << "CR DC (4):   ";
        for (int j = 20; j < 24; j++) {
            out << setw(4) << data.dc_coeffs[j] << " ";
        }
        out << "\n\n";
    }

    if (dc_data.size() > print_count) {
        out << "... (remaining " << (dc_data.size() - print_count) << " NALUs omitted)\n";
    }

    out.close();
    cout << "[✓] Saved to: " << output_file << "\n";
}

// Print comparison between original and encrypted DC
void print_dc_comparison(const vector<DCData>& original, const vector<DCData>& encrypted, const string& output_file) {
    ofstream out(output_file);
    if (!out) {
        cerr << "Error: Cannot create comparison file: " << output_file << "\n";
        return;
    }

    out << "╔═══════════════════════════════════════════════════════════════════╗\n";
    out << "║              ORIGINAL vs ENCRYPTED DC COMPARISON                  ║\n";
    out << "╚═══════════════════════════════════════════════════════════════════╝\n\n";

    // Compare first 10 NALUs
    int compare_count = min(10, (int)min(original.size(), encrypted.size()));

    for (int i = 0; i < compare_count; i++) {
        const auto& orig = original[i];
        const auto& enc = encrypted[i];

        out << "═══════════════════════════════════════════════════════════════════\n";
        out << "NALU #" << i << "\n";
        out << "───────────────────────────────────────────────────────────────────\n";

        out << "LUMA DC Comparison:\n";
        for (int j = 0; j < 16; j++) {
            out << "  [" << j << "] Original: " << setw(4) << orig.dc_coeffs[j]
                << " → Encrypted: " << setw(4) << enc.dc_coeffs[j];
            if (orig.dc_coeffs[j] != enc.dc_coeffs[j]) {
                out << " (CHANGED ✓)";
            }
            out << "\n";
        }
        out << "\n";
    }

    out.close();
    cout << "[✓] Saved comparison to: " << output_file << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <original.h264> [encrypted.h264]\n";
        cout << "\nExample:\n";
        cout << "  " << argv[0] << " output.h264\n";
        cout << "  " << argv[0] << " output.h264 output_en.h264\n";
        return 1;
    }

    string original_file = argv[1];
    string encrypted_file = (argc >= 3) ? argv[2] : "";

    // Extract DC from original
    string original_dc_file = original_file + ".dc";
    cout << "[*] Reading DC metadata from: " << original_dc_file << "\n";
    auto original_dc = read_dc_from_metadata(original_dc_file);

    if (original_dc.empty()) {
        cerr << "Error: Could not read DC metadata. Have you run encryption?\n";
        return 1;
    }

    cout << "[✓] Extracted " << original_dc.size() << " NALU DC values\n\n";

    // Print original DC coefficients
    string output_original = original_file + "_dc_original.txt";
    print_dc_coefficients(original_dc, output_original);

    // If encrypted file provided, compare
    if (!encrypted_file.empty()) {
        string encrypted_dc_file = encrypted_file + ".dc";
        cout << "[*] Reading DC metadata from: " << encrypted_dc_file << "\n";
        auto encrypted_dc = read_dc_from_metadata(encrypted_dc_file);

        if (encrypted_dc.empty()) {
            cerr << "Error: Could not read encrypted DC metadata\n";
            return 1;
        }

        cout << "[✓] Extracted " << encrypted_dc.size() << " encrypted NALU DC values\n\n";

        // Print encrypted DC coefficients
        string output_encrypted = encrypted_file + "_dc_encrypted.txt";
        print_dc_coefficients(encrypted_dc, output_encrypted);

        // Print comparison
        string output_comparison = original_file + "_dc_comparison.txt";
        print_dc_comparison(original_dc, encrypted_dc, output_comparison);
    }

    cout << "\n[DONE] DC extraction complete\n";
    return 0;
}
