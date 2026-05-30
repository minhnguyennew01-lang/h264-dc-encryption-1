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
    
    for (size_t i = 0; i < file_data.size() - 2; i++) {
        size_t nal_offset = -1;
        
        if (i + 3 < file_data.size() && 
            file_data[i] == 0x00 && file_data[i+1] == 0x00 && 
            file_data[i+2] == 0x00 && file_data[i+3] == 0x01) {
            nal_offset = i + 4;
            i += 3;
        }
        else if (i + 2 < file_data.size() &&
                 file_data[i] == 0x00 && file_data[i+1] == 0x00 && 
                 file_data[i+2] == 0x01) {
            nal_offset = i + 3;
        }
        
        if (nal_offset < file_data.size()) {
            uint8_t nal_byte = file_data[nal_offset];
            int nal_type = nal_byte & 0x1F;
            
            if (nal_type == 1) {
                type1_count++;
                if (type1_count > 18882) {  // Print extra ones
                    cout << "Extra Type 1 #" << type1_count << " at offset " << nal_offset 
                         << " (start code at " << (nal_offset - 4) << ")" << endl;
                    cout << "  Bytes before: ";
                    for (int j = -10; j < 0; j++) {
                        if (nal_offset + j >= 0)
                            printf("%02x ", file_data[nal_offset + j]);
                    }
                    cout << endl;
                    cout << "  NAL byte: " << (int)nal_byte << " (binary: ";
                    for (int b = 7; b >= 0; b--)
                        cout << ((nal_byte >> b) & 1);
                    cout << ")" << endl;
                }
            }
        }
    }
    
    cout << "Total Type 1: " << type1_count << endl;
    
    return 0;
}
