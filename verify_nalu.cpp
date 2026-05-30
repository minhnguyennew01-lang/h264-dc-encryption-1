#include <cstdio>
#include <cstdint>
#include <cstring>
#include <map>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <h264_file>\n", argv[0]);
        return 1;
    }

    FILE* fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    uint8_t* file_data = nullptr;
    size_t file_size = 0;
    
    // Read entire file
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    file_data = new uint8_t[file_size];
    fread(file_data, 1, file_size, fp);
    fclose(fp);
    
    std::map<int, int> type_count;
    int nalu_count = 0;

    for (size_t i = 0; i < file_size - 2; i++) {
        int nal_offset = -1;
        
        // Check for 4-byte start code: 0x00000001
        if (i + 3 < file_size && 
            file_data[i] == 0x00 && file_data[i+1] == 0x00 && 
            file_data[i+2] == 0x00 && file_data[i+3] == 0x01) {
            nal_offset = i + 4;
        }
        // Check for 3-byte start code: 0x000001
        else if (i + 2 < file_size &&
                 file_data[i] == 0x00 && file_data[i+1] == 0x00 && 
                 file_data[i+2] == 0x01) {
            // Only if previous byte is not 0x00 (to avoid double-counting with 4-byte)
            if (i == 0 || file_data[i-1] != 0x00) {
                nal_offset = i + 3;
            }
        }
        
        if (nal_offset > 0 && nal_offset < file_size) {
            uint8_t nal_byte = file_data[nal_offset];
            int nal_type = nal_byte & 0x1F;
            type_count[nal_type]++;
            nalu_count++;
        }
    }
    
    delete[] file_data;

    printf("File: %s\n", argv[1]);
    printf("Total NALUs: %d\n\n", nalu_count);
    printf("NAL Type Distribution:\n");
    for (auto& p : type_count) {
        printf("  Type %d: %d\n", p.first, p.second);
    }

    return 0;
}
