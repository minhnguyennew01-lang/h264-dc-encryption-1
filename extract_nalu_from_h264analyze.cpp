#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cstring>
using namespace std;

struct NALUInfo {
    size_t start_pos;      // Vị trí start code
    size_t nal_pos;        // Vị trí NAL byte
    int type;              // NALU type (1-8)
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <input.h264>\n";
        return 1;
    }

    string input_file = argv[1];
    
    // Get h264_analyze binary path from environment or use default
    // (set by extract_nalu_from_h264analyze_wrapper.sh)
    const char* h264_bin_env = getenv("H264_ANALYZE_BIN");
    string h264_bin = h264_bin_env ? string(h264_bin_env) : "./third_party/h264bitstream/.libs/h264_analyze";
    
    // Build command with proper LD_LIBRARY_PATH
    string cmd = string("export LD_LIBRARY_PATH=./third_party/h264bitstream/.libs:/usr/local/lib:$LD_LIBRARY_PATH && ") +
                 "timeout 300 " + h264_bin + " " + input_file + " 2>&1";
    
    // DEBUG: print the command being executed when env var EXTRACT_DEBUG is set
    const char* debug_env = getenv("EXTRACT_DEBUG");
    if (debug_env && string(debug_env) == "1") {
        cerr << "DEBUG: running cmd: " << cmd << "\n";
    }
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        cerr << "Error running h264_analyze\n";
        return 1;
    }

    vector<NALUInfo> nalus;
    char line[1024];
    
    while (fgets(line, sizeof(line), pipe)) {
        // Look for: "nal_unit_type: X"
        char* type_str = strstr(line, "nal_unit_type:");
        if (type_str) {
            int nal_type = -1;
            sscanf(type_str, "nal_unit_type: %d", &nal_type);
            if (nal_type >= 0 && nal_type <= 31) {
                // We'll extract positions later when reading the file
                NALUInfo info;
                info.type = nal_type;
                nalus.push_back(info);
            }
        }
    }
    pclose(pipe);

    // Now read the actual file to find start code positions
    ifstream input(input_file, ios::binary);
    if (!input) {
        cerr << "Cannot open: " << input_file << "\n";
        return 1;
    }
    
    vector<uint8_t> file_data((istreambuf_iterator<char>(input)), 
                              istreambuf_iterator<char>());
    input.close();

    // Find all start codes and match with h264_analyze types
    vector<NALUInfo> found_nalus;
    int type_index = 0;
    
    for (size_t i = 0; i < file_data.size() - 3; i++) {
        size_t start_pos = -1;
        size_t nal_pos = -1;
        
        // 4-byte start code
        if (file_data[i] == 0x00 && file_data[i+1] == 0x00 && 
            file_data[i+2] == 0x00 && file_data[i+3] == 0x01) {
            start_pos = i;
            nal_pos = i + 4;
            i += 3;
        }
        // 3-byte start code
        else if (i + 2 < file_data.size() &&
                 file_data[i] == 0x00 && file_data[i+1] == 0x00 && 
                 file_data[i+2] == 0x01) {
            start_pos = i;
            nal_pos = i + 3;
        }
        
        if (nal_pos < file_data.size()) {
            uint8_t nal_byte = file_data[nal_pos];
            int detected_type = nal_byte & 0x1F;
            
            // Match with h264_analyze order
            if (type_index < nalus.size() && detected_type == nalus[type_index].type) {
                NALUInfo info;
                info.start_pos = start_pos;
                info.nal_pos = nal_pos;
                info.type = detected_type;
                found_nalus.push_back(info);
                type_index++;
            }
        }
    }

    // Write NALU info to file for pipelines to use
    ofstream nalu_file("nalu_info.bin", ios::binary);
    uint32_t count = found_nalus.size();
    nalu_file.write((char*)&count, sizeof(count));
    
    for (const auto& nalu : found_nalus) {
        uint32_t start = nalu.start_pos;
        uint32_t nal = nalu.nal_pos;
        uint8_t type = nalu.type;
        nalu_file.write((char*)&start, sizeof(start));
        nalu_file.write((char*)&nal, sizeof(nal));
        nalu_file.write((char*)&type, sizeof(type));
    }
    nalu_file.close();

    cout << "✅ Extracted " << found_nalus.size() << " NALUs from h264_analyze\n";
    cout << "📄 Saved to: nalu_info.bin\n";
    
    return 0;
}
