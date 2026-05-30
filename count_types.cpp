#include <iostream>
#include <fstream>
#include <cstdint>
using namespace std;

int main() {
    ifstream f("nalu_info.bin", ios::binary);
    uint32_t count = 0;
    f.read((char*)&count, sizeof(count));
    
    int type1 = 0, type5 = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t start, nal;
        uint8_t type;
        f.read((char*)&start, sizeof(start));
        f.read((char*)&nal, sizeof(nal));
        f.read((char*)&type, sizeof(type));
        
        if (type == 1) type1++;
        else if (type == 5) type5++;
    }
    
    cout << "Type 1: " << type1 << endl;
    cout << "Type 5: " << type5 << endl;
    cout << "Type 1 + 5: " << (type1 + type5) << endl;
    
    return 0;
}
