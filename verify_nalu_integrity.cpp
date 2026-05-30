#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <file.h264>\n";
        return 1;
    }
    
    ifstream input(argv[1], ios::binary);
    vector<uint8_t> file_data((istreambuf_iterator<char>(input)), istreambuf_iterator<char>());
    input.close();
    
    cout << "File: " << argv[1] << " (" << file_data.size() << " bytes)\n";
    cout << "NALU start code analysis:\n";
    
    int four_byte_count = 0, three_byte_count = 0;
    int total_nalus = 0;
    
    for (size_t i = 0; i < file_data.size() - 3; i++) {
        if (file_data[i] == 0x00 && file_data[i+1] == 0x00 && 
            file_data[i+2] == 0x00 && file_data[i+3] == 0x01) {
            four_byte_count++;
            total_nalus++;
            i += 3;
        }
        else if (i + 2 < file_data.size() &&
                 file_data[i] == 0x00 && file_data[i+1] == 0x00 && 
                 file_data[i+2] == 0x01) {
            three_byte_count++;
            total_nalus++;
        }
    }
    
    cout << "4-byte start codes: " << four_byte_count << endl;
    cout << "3-byte start codes: " << three_byte_count << endl;
    cout << "Total: " << total_nalus << endl;
    
    return 0;
}
