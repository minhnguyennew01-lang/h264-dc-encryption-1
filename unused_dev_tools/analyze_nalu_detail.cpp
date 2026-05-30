#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <map>

void analyze_nalu(const char* filename, int max_nalus = 20) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        perror("fopen");
        return;
    }

    uint8_t buffer[8192];
    size_t bytes_read;
    std::map<int, int> type_count;
    int nalu_count = 0;
    
    printf("================================================================================\n");
    printf("NALU Analysis: %s\n", filename);
    printf("================================================================================\n\n");

    std::vector<std::pair<size_t, int>> nalu_info;  // offset, type
    size_t total_offset = 0;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        for (size_t i = 0; i < bytes_read - 4; i++) {
            if (buffer[i] == 0x00 && buffer[i+1] == 0x00 && 
                buffer[i+2] == 0x00 && buffer[i+3] == 0x01) {
                
                if (i + 4 < bytes_read) {
                    uint8_t nal_byte = buffer[i + 4];
                    int nal_type = nal_byte & 0x1F;
                    
                    nalu_info.push_back({total_offset + i, nal_type});
                    type_count[nal_type]++;
                    nalu_count++;
                    
                    if (nalu_count <= max_nalus) {
                        // Show DC location (offset 20 from NALU start)
                        size_t dc_offset = total_offset + i + 4 + 20;
                        printf("NALU %3d | Type %d | Offset 0x%08lx | DC starts at 0x%08lx\n",
                               nalu_count, nal_type, total_offset + i, dc_offset);
                    }
                }
            }
        }
        total_offset += bytes_read - 4;  // Account for overlap in next iteration
    }

    fclose(fp);

    printf("\n" "================================================================================\n");
    printf("Summary:\n");
    printf("Total NALUs: %d\n\n", nalu_count);
    printf("Type Distribution:\n");
    for (auto& p : type_count) {
        printf("  Type %d: %d\n", p.first, p.second);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <h264_file> [max_nalus_to_show]\n", argv[0]);
        return 1;
    }

    int max_nalus = argc > 2 ? atoi(argv[2]) : 20;
    analyze_nalu(argv[1], max_nalus);

    return 0;
}
