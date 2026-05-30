#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
using namespace std;

int main() {
    string input_file = "output.h264";
    ifstream input(input_file, ios::binary);
    vector<uint8_t> file_data((istreambuf_iterator<char>(input)), istreambuf_iterator<char>());
    input.close();
    
    int type1_count = 0;
    int false_positives = 0;
    
    for (size_t i = 0; i < file_data.size() - 2; i++) {
        size_t nal_offset = -1;
        
        // 4-byte start code
        if (i + 3 < file_data.size() && 
            file_data[i] == 0x00 && file_data[i+1] == 0x00 && 
            file_data[i+2] == 0x00 && file_data[i+3] == 0x01) {
            nal_offset = i + 4;
            i += 3;
        }
        // 3-byte start code with EPB check
        else if (i + 2 < file_data.size() &&
                 file_data[i] == 0x00 && file_data[i+1] == 0x00 && 
                 file_data[i+2] == 0x01) {
            
            bool is_after_epb = false;
            if (i > 0 && file_data[i-1] == 0x03 && i >= 2 &&
                file_data[i-2] == 0x00 && file_data[i-3] == 0x00) {
                is_after_epb = true;
            }
            
            if (!is_after_epb) {
                nal_offset = i + 3;
            } else {
                false_positives++;
                cout << "Skipped EPB-related false positive at offset " << i << endl;
            }
        }
        
        if (nal_offset < file_data.size()) {
            uint8_t nal_byte = file_data[nal_offset];
            int nal_type = nal_byte & 0x1F;
            
            if (nal_type == 1) {
                type1_count++;
                if (type1_count > 18882) {
                    cout << "Type 1 #" << type1_count << " at offset " << nal_offset << endl;
                }
            }
        }
    }
    
    cout << "Total Type 1: " << type1_count << endl;
    cout << "False positives skipped: " << false_positives << endl;
    
    return 0;
}
