#include "dc_metadata.h"
#include <fstream>
#include <sstream>
#include <iostream>

void save_dc_metadata_with_padding(
    const std::string& filename,
    const std::vector<DCMetadataWithPadding>& metadata
) {
    std::ofstream file(filename);
    if (!file) {
        std::cerr << "Error: Cannot open metadata file for writing: " << filename << "\n";
        return;
    }
    
    // Header with column names (added extra_byte_encrypted)
    file << "nalu_index,y_start,y_count,cb_start,cb_count,cr_start,cr_count,"
         << "y_padding,cb_padding,cr_padding,extra_byte_encrypted\n";
    
    // Data rows
    for (const auto& meta : metadata) {
        file << meta.nalu_index << ","
             << meta.y_start << "," << meta.y_count << ","
             << meta.cb_start << "," << meta.cb_count << ","
             << meta.cr_start << "," << meta.cr_count << ","
             << (int)meta.y_padding << ","
             << (int)meta.cb_padding << ","
             << (int)meta.cr_padding << ","
             << (int)meta.extra_byte_encrypted << "\n";
    }
    
    file.close();
    std::cout << "✅ Saved metadata with padding: " << filename << "\n";
}

std::vector<DCMetadataWithPadding> load_dc_metadata_with_padding(
    const std::string& filename
) {
    std::vector<DCMetadataWithPadding> metadata;
    std::ifstream file(filename);
    
    if (!file) {
        std::cerr << "Error: Cannot open metadata file for reading: " << filename << "\n";
        return metadata;
    }
    
    std::string line;
    
    // Skip header
    std::getline(file, line);
    
    // Read data rows
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::istringstream iss(line);
        DCMetadataWithPadding meta;
        char comma;
        int y_pad, cb_pad, cr_pad, extra_byte;
        
        iss >> meta.nalu_index >> comma
            >> meta.y_start >> comma >> meta.y_count >> comma
            >> meta.cb_start >> comma >> meta.cb_count >> comma
            >> meta.cr_start >> comma >> meta.cr_count >> comma
            >> y_pad >> comma
            >> cb_pad >> comma
            >> cr_pad >> comma
            >> extra_byte;
        
        meta.y_padding = (uint8_t)y_pad;
        meta.cb_padding = (uint8_t)cb_pad;
        meta.cr_padding = (uint8_t)cr_pad;
        meta.extra_byte_encrypted = (uint8_t)extra_byte;
        
        metadata.push_back(meta);
    }
    
    file.close();
    std::cout << "✅ Loaded " << metadata.size() << " metadata entries with padding\n";
    
    return metadata;
}
