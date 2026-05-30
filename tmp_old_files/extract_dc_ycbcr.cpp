#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <sstream>

using namespace std;

struct DCData {
    uint32_t nalu_index;
    uint8_t nalu_type;
    vector<int> dc_y;     // 16 LUMA
    vector<int> dc_cb;    // 4 CB
    vector<int> dc_cr;    // 4 CR
};

// Parse current format: NALU [Type:X] Y: val1 val2 ... | Cb: val1 val2 val3 val4 | Cr: val1 val2 val3 val4
vector<DCData> parse_dc_file(const string& filename) {
    ifstream file(filename);
    if (!file) {
        cerr << "Error: Cannot open DC file: " << filename << "\n";
        return {};
    }

    vector<DCData> dc_data;
    string line;
    uint32_t nalu_index = 0;

    while (getline(file, line)) {
        // Skip header and empty lines
        if (line.empty() || line[0] == '#' || line[0] == '═') continue;

        // Parse: NALU [Type:X] Y: ... | Cb: ... | Cr: ...
        size_t type_pos = line.find("Type:");
        if (type_pos == string::npos) continue;

        // Extract NALU type
        uint8_t nalu_type = stoul(line.substr(type_pos + 5, 1));

        // Extract Y values (16)
        size_t y_start = line.find("Y:") + 2;
        size_t cb_start = line.find("| Cb:");
        string y_str = line.substr(y_start, cb_start - y_start);

        vector<int> y_vals;
        stringstream y_ss(y_str);
        int val;
        while (y_ss >> val) {
            y_vals.push_back(val);
        }

        // Extract Cb values (4)
        size_t cb_val_start = cb_start + 5;
        size_t cr_start = line.find("| Cr:");
        string cb_str = line.substr(cb_val_start, cr_start - cb_val_start);

        vector<int> cb_vals;
        stringstream cb_ss(cb_str);
        while (cb_ss >> val) {
            cb_vals.push_back(val);
        }

        // Extract Cr values (4)
        size_t cr_val_start = cr_start + 5;
        string cr_str = line.substr(cr_val_start);

        vector<int> cr_vals;
        stringstream cr_ss(cr_str);
        while (cr_ss >> val) {
            cr_vals.push_back(val);
        }

        // Ensure correct sizes
        while (y_vals.size() < 16) y_vals.push_back(0);
        while (cb_vals.size() < 4) cb_vals.push_back(0);
        while (cr_vals.size() < 4) cr_vals.push_back(0);

        y_vals.resize(16);
        cb_vals.resize(4);
        cr_vals.resize(4);

        DCData data;
        data.nalu_index = nalu_index++;
        data.nalu_type = nalu_type;
        data.dc_y = y_vals;
        data.dc_cb = cb_vals;
        data.dc_cr = cr_vals;
        dc_data.push_back(data);
    }

    file.close();
    return dc_data;
}

// Print DC coefficients with Y/Cb/Cr separation
void print_dc_coefficients(const vector<DCData>& dc_data, const string& output_file) {
    ofstream out(output_file);
    if (!out) {
        cerr << "Error: Cannot create output file: " << output_file << "\n";
        return;
    }

    out << "╔════════════════════════════════════════════════════════════════════════╗\n";
    out << "║           DC COEFFICIENTS ANALYSIS (Y/Cb/Cr Separated)                ║\n";
    out << "╚════════════════════════════════════════════════════════════════════════╝\n\n";

    out << "Total NALUs: " << dc_data.size() << "\n";
    out << "DC per NALU: 24 (16 Y + 4 Cb + 4 Cr)\n\n";

    // Print all NALUs
    for (size_t i = 0; i < dc_data.size(); i++) {
        const auto& data = dc_data[i];
        out << "════════════════════════════════════════════════════════════════════════\n";
        out << "NALU #" << data.nalu_index << " [Type:" << (int)data.nalu_type << "]\n";
        out << "────────────────────────────────────────────────────────────────────────\n";

        out << "Y (Luma - 16 values):  ";
        for (int j = 0; j < 16; j++) {
            out << setw(5) << data.dc_y[j] << " ";
        }
        out << "\n";

        out << "Cb (Chroma Blue - 4):  ";
        for (int j = 0; j < 4; j++) {
            out << setw(5) << data.dc_cb[j] << " ";
        }
        out << "\n";

        out << "Cr (Chroma Red - 4):   ";
        for (int j = 0; j < 4; j++) {
            out << setw(5) << data.dc_cr[j] << " ";
        }
        out << "\n\n";
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

    out << "╔════════════════════════════════════════════════════════════════════════╗\n";
    out << "║             ORIGINAL vs ENCRYPTED DC COMPARISON                        ║\n";
    out << "╚════════════════════════════════════════════════════════════════════════╝\n\n";

    int compare_count = min(10, (int)min(original.size(), encrypted.size()));

    for (int i = 0; i < compare_count; i++) {
        const auto& orig = original[i];
        const auto& enc = encrypted[i];

        out << "════════════════════════════════════════════════════════════════════════\n";
        out << "NALU #" << i << " [Type:" << (int)orig.nalu_type << "]\n";
        out << "────────────────────────────────────────────────────────────────────────\n";

        out << "Y (Luma) Comparison:\n";
        int y_changed = 0;
        for (int j = 0; j < 16; j++) {
            out << "  [" << j << "] Original: " << setw(5) << orig.dc_y[j]
                << " → Encrypted: " << setw(5) << enc.dc_y[j];
            if (orig.dc_y[j] != enc.dc_y[j]) {
                out << " ✓";
                y_changed++;
            }
            out << "\n";
        }
        out << "Y Changed: " << y_changed << "/16\n\n";

        out << "Cb (Chroma Blue) Comparison:\n";
        int cb_changed = 0;
        for (int j = 0; j < 4; j++) {
            out << "  [" << j << "] Original: " << setw(5) << orig.dc_cb[j]
                << " → Encrypted: " << setw(5) << enc.dc_cb[j];
            if (orig.dc_cb[j] != enc.dc_cb[j]) {
                out << " ✓";
                cb_changed++;
            }
            out << "\n";
        }
        out << "Cb Changed: " << cb_changed << "/4\n\n";

        out << "Cr (Chroma Red) Comparison:\n";
        int cr_changed = 0;
        for (int j = 0; j < 4; j++) {
            out << "  [" << j << "] Original: " << setw(5) << orig.dc_cr[j]
                << " → Encrypted: " << setw(5) << enc.dc_cr[j];
            if (orig.dc_cr[j] != enc.dc_cr[j]) {
                out << " ✓";
                cr_changed++;
            }
            out << "\n";
        }
        out << "Cr Changed: " << cr_changed << "/4\n\n";
    }

    out.close();
    cout << "[✓] Saved comparison to: " << output_file << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "DC Coefficient Analysis Tool (Y/Cb/Cr Format)\n";
        cout << "Usage: " << argv[0] << " <dc_before.txt> [dc_after.txt]\n";
        cout << "\nExample:\n";
        cout << "  " << argv[0] << " test.h264.dc_before.txt\n";
        cout << "  " << argv[0] << " test.h264.dc_before.txt test.h264.dc_after.txt\n";
        return 1;
    }

    string before_file = argv[1];
    string after_file = (argc >= 3) ? argv[2] : "";

    cout << "[*] Reading DC coefficients from: " << before_file << "\n";
    auto before_dc = parse_dc_file(before_file);

    if (before_dc.empty()) {
        cerr << "Error: Could not parse DC file\n";
        return 1;
    }

    cout << "[✓] Extracted " << before_dc.size() << " NALU DC values\n\n";

    // Print analysis
    string output_before = before_file + "_analysis.txt";
    print_dc_coefficients(before_dc, output_before);

    // If after file provided, compare
    if (!after_file.empty()) {
        cout << "[*] Reading DC coefficients from: " << after_file << "\n";
        auto after_dc = parse_dc_file(after_file);

        if (after_dc.empty()) {
            cerr << "Error: Could not parse DC file\n";
            return 1;
        }

        cout << "[✓] Extracted " << after_dc.size() << " NALU DC values\n\n";

        // Print analysis
        string output_after = after_file + "_analysis.txt";
        print_dc_coefficients(after_dc, output_after);

        // Print comparison
        string output_comparison = before_file + "_vs_" + after_file + "_comparison.txt";
        print_dc_comparison(before_dc, after_dc, output_comparison);
    }

    cout << "\n[DONE] DC analysis complete\n";
    return 0;
}
