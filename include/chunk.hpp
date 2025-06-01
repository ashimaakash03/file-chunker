// include/chunk.hpp
#pragma once

#include <vector>
#include <string>
#include <filesystem>
#include <stdexcept>

#include "chunk_config.hpp"
#include "cid_utility.hpp"

namespace FileManager {
namespace Chunks {

class Chunk {
public:
    std::vector<char> data; // The actual content of the chunk
    std::string cid;        // The Content Identifier (SHA-256 hash)

    // Constructor to create a chunk from data and generate its CID
    Chunk(std::vector<char> chunk_data) : data(std::move(chunk_data)) {
        cid = CID::CIDUtility::generateSHA256(data);
    }

    // Default constructor for loading
    Chunk() = default;

    // Save the chunk to disk. Filename will be its CID.
    bool save(const Config::ChunkConfig& config);

    // Static method to load chunk data from disk given its CID.
    static std::vector<char> loadData(const Config::ChunkConfig& config, const std::string& chunk_cid);

    // Get the full path where this chunk would be stored
    std::filesystem::path getFullPath(const Config::ChunkConfig& config) const;
};

} // namespace Chunks
} // namespace FileManager