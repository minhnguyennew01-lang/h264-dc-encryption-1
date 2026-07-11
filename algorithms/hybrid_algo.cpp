#include "hybrid_algo.h"

std::vector<uint8_t> HybridAlgo::encrypt(
    const std::vector<uint8_t>& dc_data,
    const std::vector<uint8_t>& key,
    int nalu_index)
{
    uint8_t padding_byte = 0;
    return HybridEncryption::encrypt_hybrid(dc_data, key, nalu_index, padding_byte);
}

std::vector<uint8_t> HybridAlgo::decrypt(
    const std::vector<uint8_t>& encrypted_data,
    const std::vector<uint8_t>& key,
    int nalu_index)
{
    // padding_byte = 0 matches original pipeline behaviour (it was always 0 there too)
    return HybridEncryption::decrypt_hybrid(encrypted_data, key, nalu_index, 0);
}
