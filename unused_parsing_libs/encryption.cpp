#include "encryption.h"
#include <cstring>
#include <iostream>
#include <fstream>
#include <cmath>

using namespace std;

// KeystreamGenerator implementation
void KeystreamGenerator::generateKeystream(const std::vector<uint8_t>& key,
                                          size_t length,
                                          std::vector<uint8_t>& keystream) {
    keystream.resize(length);
    
    if (key.empty()) return;
    
    // Simple PLCM-based keystream generation
    double mu = 0.37;
    double x = 0.2;
    
    // Pre-iterate
    for (int i = 0; i < 1000; i++) {
        if (x < mu) {
            x = x / mu;
        } else {
            x = (1.0 - x) / (1.0 - mu);
        }
    }
    
    // Generate keystream
    for (size_t i = 0; i < length; i++) {
        // Iterate PLCM
        if (x < mu) {
            x = x / mu;
        } else {
            x = (1.0 - x) / (1.0 - mu);
        }
        
        // Extract byte from x
        uint8_t byte_val = (uint8_t)(((int)(x * 256)) & 0xFF);
        
        // Mix with key
        keystream[i] = byte_val ^ key[i % key.size()];
    }
}

// PLCM (Piecewise Linear Chaotic Map) Implementation
class PLCM {
public:
    double x;
    double mu;
    
    PLCM(double mu_val, double x_val) : x(x_val), mu(mu_val) {
        // Adjust mu if needed
        if (mu >= 0.5) mu = 1.0 - mu;
        
        // Pre-iterate 1000 times to stabilize
        for (int i = 0; i < 1000; i++) {
            x = single_iterate(x);
        }
    }
    
    double single_iterate(double x_val) {
        if (x_val < mu) {
            return x_val / mu;
        } else {
            return (1.0 - x_val) / (1.0 - mu);
        }
    }
    
    vector<uint8_t> double_to_bytes(double val) {
        vector<uint8_t> result(6);
        for (int i = 0; i < 6; i++) {
            val *= 256.0;
            int byte_val = (int)val;
            result[i] = (uint8_t)(byte_val & 0xFF);
            val -= byte_val;
        }
        return result;
    }
};

// Cat Map Transformation for Confusion
void cat_map_confusion(vector<uint8_t>& data, int size) {
    vector<uint8_t> temp = data;
    
    // Use simple cyclic shift as Cat Map
    // This is a valid permutation that is easily invertible
    for (int i = 0; i < size; i++) {
        int new_pos = (i + 1) % size;
        data[new_pos] = temp[i];
    }
}

// Inverse Cat Map for Decryption
void inverse_cat_map_confusion(vector<uint8_t>& data, int size) {
    vector<uint8_t> temp = data;
    
    // Inverse of cyclic shift
    for (int i = 0; i < size; i++) {
        int orig_pos = (i - 1 + size) % size;
        data[orig_pos] = temp[i];
    }
}

// Diffusion Operation using PLCM
void diffusion_operation(vector<uint8_t>& data, const vector<uint8_t>& keystream) {
    int size = data.size();
    vector<uint8_t> result(size);
    
    for (int i = 0; i < size; i++) {
        uint8_t original = data[i];
        uint8_t key_byte = keystream[i % keystream.size()];
        
        if (i == 0) {
            // First byte: XOR with keystream
            result[i] = original ^ key_byte;
        } else {
            // Other bytes: XOR with keystream and previous result
            result[i] = original ^ key_byte ^ result[i - 1];
        }
    }
    
    data = result;
}

// Inverse Diffusion Operation
void inverse_diffusion_operation(vector<uint8_t>& encrypted_data, const vector<uint8_t>& keystream) {
    int size = encrypted_data.size();
    vector<uint8_t> result(size);
    
    // Process in REVERSE order (from end to beginning)
    for (int i = size - 1; i >= 0; i--) {
        uint8_t encrypted = encrypted_data[i];
        uint8_t key_byte = keystream[i % keystream.size()];
        
        if (i == 0) {
            // First byte: XOR with keystream only
            result[i] = encrypted ^ key_byte;
        } else {
            // Other bytes: XOR with keystream and previous encrypted byte
            // During encryption: encrypted[i] = original[i] ^ key_byte[i] ^ encrypted[i-1]
            // So: original[i] = encrypted[i] ^ key_byte[i] ^ encrypted[i-1]
            result[i] = encrypted ^ key_byte ^ encrypted_data[i - 1];
        }
    }
    
    encrypted_data = result;
}

Encryption::Encryption(const vector<uint8_t>& key, int mb_index)
    : key_(key), mb_index_(mb_index) {
    keystream_ = generate_keystream(key, mb_index);
}

// Keystream generation using PLCM
uint8_t Encryption::generate_keystream(const vector<uint8_t>& key, int mb_index) {
    // Use key bytes to derive PLCM parameters
    double mu = 0.0;
    double init_x = 0.0;
    
    for (size_t i = 0; i < key.size(); ++i) {
        mu += key[i] / 256.0;
    }
    mu = fmod(mu, 1.0);
    if (mu >= 0.5) mu = 1.0 - mu;
    
    // Use mb_index to derive initial condition
    init_x = fmod((mb_index * 0.123456789), 1.0);
    
    // Create PLCM and iterate
    PLCM plcm(mu, init_x);
    plcm.x = plcm.single_iterate(plcm.x);
    
    // Extract byte from chaotic value
    vector<uint8_t> bytes = plcm.double_to_bytes(plcm.x);
    return bytes[0];
}

int Encryption::encrypt_dc(int dc) {
    // Use PLCM-generated keystream with diffusion
    int encrypted = (dc + keystream_) % 256;
    if (encrypted < 0) encrypted += 256;
    if (encrypted > 127) encrypted = encrypted - 256;
    return encrypted;
}

int Encryption::decrypt_dc(int dc) {
    // Reverse the diffusion
    int decrypted = (dc - keystream_ + 256) % 256;
    if (decrypted > 127) decrypted = decrypted - 256;
    return decrypted;
}

int Encryption::encrypt_mv(int mv) {
    // Use PLCM keystream for motion vector encryption
    int ks = (int)keystream_;
    int encrypted = (mv + ks) % 256;
    if (encrypted < 0) encrypted += 256;
    if (encrypted > 127) encrypted = encrypted - 256;
    return encrypted;
}

int Encryption::decrypt_mv(int mv_enc) {
    // Reverse the additive encryption
    int decrypted = (mv_enc - (int)keystream_ + 256) % 256;
    if (decrypted < 0) decrypted += 256;
    if (decrypted > 127) decrypted = decrypted - 256;
    return decrypted;
}

// Helper: Derive initial PLCM x from (key, nalu_index)
double derive_plcm_init_x(const vector<uint8_t>& key, int nalu_index) {
    double x = 0.0;
    // Mix key bytes with nalu_index
    for (size_t i = 0; i < key.size(); ++i) {
        x += (key[i] * (1.0 / 256.0)) * (1.0 + nalu_index * 0.001);
    }
    x += nalu_index * 0.0123456789;
    x = fmod(x, 1.0);
    if (x < 0) x += 1.0;
    // Ensure x is in valid range
    if (x >= 1.0) x = fmod(x, 1.0);
    if (x <= 0.0) x = 0.5;
    return x;
}

// Encrypt entire NALU using PLCM Confusion + Diffusion algorithm
vector<uint8_t> encrypt_dc_coefficients(
    const vector<uint8_t>& nalu,
    const vector<uint8_t>& key,
    int nalu_index
) {
    vector<uint8_t> encrypted = nalu;
    
    if (encrypted.size() <= 4) {
        return encrypted;
    }
    
    // Determine start code length and extract RBSP data
    int start_pos = 0;
    if (encrypted.size() >= 4 && encrypted[0] == 0x00 && encrypted[1] == 0x00 &&
        encrypted[2] == 0x00 && encrypted[3] == 0x01) {
        // 4-byte start code
        start_pos = 5;  // Skip 4-byte SC + 1-byte type
    } else if (encrypted.size() >= 3 && encrypted[0] == 0x00 && encrypted[1] == 0x00 &&
               encrypted[2] == 0x01) {
        // 3-byte start code
        start_pos = 4;  // Skip 3-byte SC + 1-byte type
    } else {
        // No start code? Treat as if position 0 is type byte
        start_pos = 1;
    }
    
    if (start_pos >= encrypted.size()) {
        return encrypted;
    }
    
    vector<uint8_t> rbsp_data(encrypted.begin() + start_pos, encrypted.end());
    
    // Generate PLCM-based keystream with NALU-specific seeding
    double mu = 0.0;
    for (size_t i = 0; i < key.size(); ++i) {
        mu += key[i] / 256.0;
    }
    mu = fmod(mu, 1.0);
    if (mu >= 0.5) mu = 1.0 - mu;
    
    // Derive initial x from key and nalu_index (FIXES NON-DETERMINISM)
    double init_x = derive_plcm_init_x(key, nalu_index);
    
    PLCM plcm(mu, init_x);  // Reset PLCM per NALU
    vector<uint8_t> keystream;
    for (size_t i = 0; i < rbsp_data.size(); i++) {
        plcm.x = plcm.single_iterate(plcm.x);
        vector<uint8_t> bytes = plcm.double_to_bytes(plcm.x);
        keystream.push_back(bytes[0]);
    }
    
    // Apply 5 rounds of Confusion + Diffusion
    for (int round = 0; round < 5; round++) {
        // Confusion: Cat Map
        cat_map_confusion(rbsp_data, rbsp_data.size());
        
        // Diffusion: XOR with PLCM keystream
        diffusion_operation(rbsp_data, keystream);
    }
    
    // Copy encrypted RBSP back
    copy(rbsp_data.begin(), rbsp_data.end(), encrypted.begin() + start_pos);
    
    return encrypted;
}


// Decrypt entire NALU using PLCM Confusion + Diffusion algorithm
vector<uint8_t> decrypt_dc_coefficients(
    const vector<uint8_t>& nalu,
    const vector<uint8_t>& key,
    int nalu_index
) {
    vector<uint8_t> decrypted = nalu;
    
    if (decrypted.size() <= 5) {
        return decrypted;
    }
    
    // Extract RBSP data (after start code + NAL header type byte)
    // Assume 4-byte start code: skip 4 bytes for start code, then 1 byte for NAL type
    vector<uint8_t> rbsp_data(decrypted.begin() + 5, decrypted.end());
    
    // Generate same PLCM-based keystream as encryption using NALU-specific seeding
    double mu = 0.0;
    for (size_t i = 0; i < key.size(); ++i) {
        mu += key[i] / 256.0;
    }
    mu = fmod(mu, 1.0);
    if (mu >= 0.5) mu = 1.0 - mu;
    
    // Derive initial x from key and nalu_index (MUST MATCH encryption)
    double init_x = derive_plcm_init_x(key, nalu_index);
    
    PLCM plcm(mu, init_x);  // Reset PLCM per NALU (same as encryption)
    vector<uint8_t> keystream;
    for (size_t i = 0; i < rbsp_data.size(); i++) {
        plcm.x = plcm.single_iterate(plcm.x);
        vector<uint8_t> bytes = plcm.double_to_bytes(plcm.x);
        keystream.push_back(bytes[0]);
    }
    
    // Apply 5 rounds of inverse Diffusion + Confusion (reverse order)
    for (int round = 0; round < 5; round++) {
        // Inverse Diffusion: reverse XOR
        inverse_diffusion_operation(rbsp_data, keystream);
        
        // Inverse Confusion: inverse Cat Map
        inverse_cat_map_confusion(rbsp_data, rbsp_data.size());
    }
    
    // Copy decrypted RBSP back
    copy(rbsp_data.begin(), rbsp_data.end(), decrypted.begin() + 5);
    
    return decrypted;
}

// Extract DC values from NALU (simplified: extract from byte positions)
std::vector<int> extract_dc_values_from_nalu(
    const std::vector<uint8_t>& nalu
) {
    std::vector<int> dc_values;
    
    // Simplified extraction: treat bytes every 16 bytes as DC samples
    // 24 DC per NALU: 16 LUMA + 4 CB + 4 CR
    // Each DC is one byte in our simplified model
    
    for (int i = 4; i < static_cast<int>(nalu.size()) && dc_values.size() < 24; i += 16) {
        if (i < static_cast<int>(nalu.size())) {
            dc_values.push_back(static_cast<int>(nalu[i]));
        }
        if (i + 1 < static_cast<int>(nalu.size()) && dc_values.size() < 24) {
            dc_values.push_back(static_cast<int>(nalu[i + 1]));
        }
    }
    
    // Pad with zeros if needed
    while (dc_values.size() < 24) {
        dc_values.push_back(0);
    }
    
    // Trim to exactly 24
    if (dc_values.size() > 24) {
        dc_values.resize(24);
    }
    
    return dc_values;
}

// Save DC values to metadata file
void save_dc_values_to_file(
    const std::string& filename,
    const std::vector<std::vector<int>>& all_dc_values
) {
    std::ofstream file(filename);
    if (!file) {
        std::cerr << "Error: Cannot create DC metadata file: " << filename << "\n";
        return;
    }
    
    for (size_t nalu_idx = 0; nalu_idx < all_dc_values.size(); nalu_idx++) {
        const auto& dc_vals = all_dc_values[nalu_idx];
        
        // Format: NALU_INDEX|DC0 DC1 DC2 ... DC23
        file << nalu_idx << "|";
        for (size_t i = 0; i < dc_vals.size(); i++) {
            if (i > 0) file << " ";
            file << dc_vals[i];
        }
        file << "\n";
    }
    
    file.close();
}


