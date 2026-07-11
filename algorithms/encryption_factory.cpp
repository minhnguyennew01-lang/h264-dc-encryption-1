#include "encryption_interface.h"
#include "hybrid_algo.h"
#include "aes_algo.h"
#include "des_algo.h"
#include "rc4_algo.h"
#include <stdexcept>

std::unique_ptr<IEncryption> IEncryption::create(const std::string& algo_name) {
    if (algo_name == "hybrid") return std::unique_ptr<IEncryption>(new HybridAlgo());
    if (algo_name == "aes")    return std::unique_ptr<IEncryption>(new AESAlgo());
    if (algo_name == "des")    return std::unique_ptr<IEncryption>(new DESAlgo());
    if (algo_name == "rc4")    return std::unique_ptr<IEncryption>(new RC4Algo());
    throw std::runtime_error(
        "Unknown algorithm: '" + algo_name + "'  (valid: hybrid | aes | des | rc4)");
}
