// include/cid_utility.hpp
#pragma once

#include <string>
#include <vector>

namespace FileManager
{
    namespace CID
    {

        class CIDUtility
        {
        public:
            // Generates SHA-256 hash of data and returns as hex string.
            // This string will serve as the Content Identifier (CID).
            static std::string generateSHA256(const std::vector<char> &data_buffer);
        };

    } // namespace CID
} // namespace FileManager