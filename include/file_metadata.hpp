// include/file_metadata.hpp
#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <chrono> // For timestamps

#include <nlohmann/json> // For JSON handling
#include "include/chunk_config.hpp"  // For directory paths

namespace FileManager {
namespace Metadata {

class FileMetadata {
public:
    std::string original_filename;
    uint64_t file_size_bytes;
    std::string content_type;
    std::string created_at; // ISO 8601 format (e.g., "YYYY-MM-DDTHH:MM:SSZ")
    std::vector<std::string> chunk_cids; // Ordered list of chunk CIDs

    // Default constructor
    FileMetadata() = default;

    // Constructor to initialize metadata
    FileMetadata(std::string filename, uint64_t size, std::string type, std::vector<std::string> cids)
        : original_filename(std::move(filename)),
          file_size_bytes(size),
          content_type(std::move(type)),
          chunk_cids(std::move(cids))
    {
        // Generate current timestamp in ISO 8601 format
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        char buf[256];
        // Format as YYYY-MM-DDTHH:MM:SSZ
        // Note: %z is for timezone offset, but for 'Z' (Zulu time), we need to handle manually
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now_c));
        created_at = buf;
    }

    // Convert FileMetadata object to nlohmann::json object
    nlohmann::json toJson() const;

    // Create FileMetadata object from nlohmann::json object
    static FileMetadata fromJson(const nlohmann::json& j);

    // Save metadata to a JSON file (filename.json)
    bool save(const Config::ChunkConfig& config) const;

    // Load metadata from a JSON file
    static FileMetadata load(const Config::ChunkConfig& config, const std::string& filename);

    // Get the full path where this metadata would be stored
    std::filesystem::path getFullPath(const Config::ChunkConfig& config) const;
};

} // namespace Metadata
} // namespace FileManager