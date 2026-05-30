#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>

struct NALUInfo {
    uint32_t position;
    uint8_t type;
    uint32_t size;
    std::string status;
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <h264_file>\n";
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open file\n";
        return 1;
    }

    std::vector<NALUInfo> nalus;
    std::vector<uint8_t> buffer;

    uint8_t byte;
    while (file.read((char*)&byte, 1)) {
        buffer.push_back(byte);
    }

    for (size_t i = 0; i < buffer.size() - 4; i++) {
        if (buffer[i] == 0x00 && buffer[i+1] == 0x00 && 
            buffer[i+2] == 0x00 && buffer[i+3] == 0x01) {
            
            if (i > 0 && nalus.size() > 0) {
                nalus.back().size = i - nalus.back().position;
            }

            uint8_t type = buffer[i+4] & 0x1F;
            NALUInfo info;
            info.position = i;
            info.type = type;
            info.size = 0;

            switch(type) {
                case 1: info.status = "P/B-slice"; break;
                case 5: info.status = "I-slice (IDR)"; break;
                case 7: info.status = "SPS"; break;
                case 8: info.status = "PPS"; break;
                default: info.status = "Other"; break;
            }

            nalus.push_back(info);
        }
    }

    if (nalus.size() > 0) {
        nalus.back().size = buffer.size() - nalus.back().position;
    }

    std::cout << "File: " << argv[1] << "\n";
    std::cout << "Total NALUs: " << nalus.size() << "\n\n";
    std::cout << "First 20 NALUs:\n";
    std::cout << "No. | Type | Position | Size | Status\n";
    std::cout << "----|------|----------|------|----------\n";

    for (size_t i = 0; i < std::min(size_t(20), nalus.size()); i++) {
        printf("%3zu | %4d | %8u | %6u | %s\n", 
               i, nalus[i].type, nalus[i].position, nalus[i].size, nalus[i].status.c_str());
    }

    int count_sps = 0, count_pps = 0, count_idr = 0, count_slice = 0;
    for (auto& n : nalus) {
        if (n.type == 7) count_sps++;
        else if (n.type == 8) count_pps++;
        else if (n.type == 5) count_idr++;
        else if (n.type == 1) count_slice++;
    }

    std::cout << "\nStatistics:\n";
    std::cout << "SPS: " << count_sps << "\n";
    std::cout << "PPS: " << count_pps << "\n";
    std::cout << "IDR: " << count_idr << "\n";
    std::cout << "Slice (P/B): " << count_slice << "\n";

    file.close();
    return 0;
}
