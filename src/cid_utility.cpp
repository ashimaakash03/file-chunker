// src/cid_utility.cpp
#include "cid_utility.hpp"
#include <iomanip>   // For std::hex, std::setw, std::setfill
#include <sstream>   // For std::stringstream
#include <stdexcept> // For std::runtime_error

// OpenSSL headers for SHA256
// Ensure you have linked OpenSSL::Crypto in your CMakeLists.txt
#include <openssl/sha.h>

namespace FileManager
{
    namespace CID
    {

        std::string CIDUtility::generateSHA256(const std::vector<char> &data_buffer)
        {
            if (data_buffer.empty())
            {
                // A hash for empty data is valid and consistent.
                // SHA256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
                unsigned char hash[SHA256_DIGEST_LENGTH];
                SHA256_CTX sha256;
                SHA256_Init(&sha256);
                SHA256_Final(hash, &sha256); // Finalize an empty context

                std::stringstream ss;
                for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
                {
                    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
                }
                return ss.str();
            }

            unsigned char hash[SHA256_DIGEST_LENGTH];
            SHA256_CTX sha256;

            if (!SHA256_Init(&sha256))
            {
                throw std::runtime_error("Failed to initialize SHA256 context.");
            }
            if (!SHA256_Update(&sha256, data_buffer.data(), data_buffer.size()))
            {
                throw std::runtime_error("Failed to update SHA256 context with data.");
            }
            if (!SHA256_Final(hash, &sha256))
            {
                throw std::runtime_error("Failed to finalize SHA256 hash calculation.");
            }

            std::stringstream ss;
            for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
            {
                ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
            }
            return ss.str();
        }

    } // namespace CID
} // namespace FileManager