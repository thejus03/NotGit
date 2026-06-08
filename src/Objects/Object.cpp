
#include "Objects/Object.hpp"
#include <openssl/evp.h>
#include <sstream>
#include <iomanip>

std::string Object::sha1() const {
    std::string data = header() + rawData();
    
    // Code is slightly more verbose but this
    // is the modern approach for OpenSSL for SHA-1
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen;
    // Create an EVP context object
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    // Set sha1 algorithm into context
    EVP_DigestInit_ex(context, EVP_sha1(), nullptr);
    // Feed the data into the sha1 algorithm
    EVP_DigestUpdate(context, data.c_str(), data.size());
    // Computes and stores encrypted data into hash array
    EVP_DigestFinal_ex(context, hash, &hashLen);
    // Free the context
    EVP_MD_CTX_free(context);


    // Convert encrypted data into hexadecimal
    std::ostringstream ss;
    for (unsigned int i = 0; i < hashLen; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return ss.str();
}