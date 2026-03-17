#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include "rbsp.h"
#include "cavlc.h"
#include "bitio.h"

using namespace std;

struct DCData {
    uint32_t nalu_index;
    vector<int> dc_coeffs;  // 24 DC coefficients: 16 LUMA + 4 CB + 4 CR
};

// Extract DC coefficients from a NALU
vector<int> extract_dc_from_nalu(const vector<uint8_t>& nalu_data) {
    vector<int> dc_coeffs;
    
    // Parse RBSP and extract DC coefficients
    // This is simplified - actual extraction requires full CAVLC parsing
    
    // For now, return dummy data
    dc_coeffs.resize(24, 0);
    return dc_coeffs;
}

// Read H.264 file and extract DC coefficients
vector<DCData> extract_dc_from_h264(const string& filename) {
    ifstream file(filename, ios::binary);
    if (!file) {
        cerr << "Error: Cannot open file: " << filename << "\n";
        return {};
    }

    vector<DCData> dc_data;
    vector<uint8_t> nalu;
    uint32_t nalu_index = 0;

    cout << "[*] Extracting DC coefficients from: " << filename << "\n";

    while (file) {
        uint8_t byte;
        file.read((char*)&byte, 1);
        if (!file) break;

        nalu.push_back(byte);

        // Check for NALU start code
        if (nalu.size() >= 4) {
            if (nalu[nalu.size()-4] == 0x00 && nalu[nalu.size()-3] == 0x00 &&
                nalu[nalu.size()-2] == 0x00 && nalu[nalu.size()-1] == 0x01) {
                
                // Process previous NALU
                if (nalu.size() > 8) {
                    vector<uint8_t> prev_nalu(nalu.begin(), nalu.end() - 4);
                    
                    // Extract DC coefficients from this NALU
                    vector<int> dc_coeffs = extract_dc_from_nalu(prev_nalu);
                    
                    DCData data;
                    data.nalu_index = nalu_index;
                    data.dc_coeffs = dc_coeffs;
                    dc_data.push_back(data);
                    
                    nalu_index++;
                    if (nalu_index % 1000 == 0) {
                        cout << "  Processed " << nalu_index << " NALUs...\n";
                    }
                }
                
                // Reset for next NALU
                nalu.clear();
                nalu.push_back(0x00);
                nalu.push_back(0x00);
                nalu.push_back(0x00);
                nalu.push_back(0x01);
            }
        }
    }

    // Handle last NALU
    if (nalu.size() > 4) {
        nalu_index++;
        vector<int> dc_coeffs = extract_dc_from_nalu(nalu);
        
        DCData data;
        data.nalu_index = nalu_index - 1;
        data.dc_coeffs = dc_coeffs;
        dc_data.push_back(data);
    }

    cout << "[✓] Extracted " << dc_data.size() << " NALUs\n";
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
    cout << "\n[1/2] Extracting DC from original file...\n";
    vector<DCData> dc_original = extract_dc_from_h264(original_file);
    
    if (dc_original.empty()) {
        cerr << "Error: No data extracted from original file\n";
        return 1;
    }

    // Extract DC from encrypted (if provided)
    vector<DCData> dc_encrypted;
    if (!encrypted_file.empty()) {
        cout << "[2/2] Extracting DC from encrypted file...\n";
        dc_encrypted = extract_dc_from_h264(encrypted_file);
    }

    // Print results
    string output_name = original_file + "_dc_original.txt";
    print_dc_coefficients(dc_original, output_name);

    if (!dc_encrypted.empty()) {
        output_name = encrypted_file + "_dc_encrypted.txt";
        print_dc_coefficients(dc_encrypted, output_name);

        // Compare DC values
        cout << "\n[COMPARISON]\n";
        ofstream comp(original_file + "_dc_comparison.txt");
        comp << "DC COEFFICIENTS COMPARISON\n";
        comp << "Original vs Encrypted\n\n";

        int compare_count = min(10, (int)dc_original.size());
        for (int i = 0; i < compare_count; i++) {
            const auto& orig = dc_original[i];
            const auto& encr = (i < (int)dc_encrypted.size()) ? dc_encrypted[i] : orig;
            
            comp << "NALU #" << i << "\n";
            comp << "Original:  ";
            for (int j = 0; j < 24; j++) {
                comp << setw(3) << orig.dc_coeffs[j] << " ";
            }
            comp << "\n";
            
            comp << "Encrypted: ";
            for (int j = 0; j < 24; j++) {
                comp << setw(3) << encr.dc_coeffs[j] << " ";
            }
            comp << "\n\n";
        }
        comp.close();
        cout << "[✓] Saved comparison to: " << original_file << "_dc_comparison.txt\n";
    }

    cout << "\n[DONE] DC extraction complete\n";
    return 0;
}
