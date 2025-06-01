// src/file_metadata.cpp
#include "file_metadata.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream> // For logging
#include <stdexcept> // For std::runtime_error

namespace fs = std::filesystem;

namespace FileManager {
namespace Metadata {

// Helper to make JSON serialization/deserialization easier using nlohmann/json macros
void to_json(nlohmann::json& j, const FileMetadata& m) {
    j = nlohmann::json{
        {"filename", m.original_filename},
        {"size", m.file_size_bytes},
        {"content_type", m.content_type},
        {"created_at", m.created_at},
        {"chunks", m.chunk_cids}
    };
}

void from_json(const nlohmann::json& j, FileMetadata& m) {
    j.at("filename").get_to(m.original_filename);
    j.at("size").get_to(m.file_size_bytes);
    j.at("content_type").get_to(m.content_type);
    j.at("created_at").get_to(m.created_at);
    j.at("chunks").get_to(m.chunk_cids);
}

nlohmann::json FileMetadata::toJson() const {
    return *this; // Uses the to_json helper function
}

FileMetadata FileMetadata::fromJson(const nlohmann::json& j) {
    FileMetadata metadata;
    j.get_to(metadata); // Uses the from_json helper function
    return metadata;
}

bool FileMetadata::save(const Config::ChunkConfig config) const {
    fs::path metadata_dir = config.getMetadataDirPath();
    fs::path metadata_path = metadata_dir / (original_filename + ".json");

    std::ofstream ofs(metadata_path);
    if (!ofs.is_open()) {
        throw std::runtime_error("Failed to open file for writing metadata: " + metadata_path.string());
    }
    ofs << toJson().dump(4); // Pretty print with 4 spaces
    if (!ofs.good()) {
        throw std::runtime_error("Failed to write all data to metadata file: " + metadata_path.string());
    }
    ofs.close();
    // std::cout << "Metadata saved: " << metadata_path.string() << std::endl;
    return true;
}

FileMetadata FileMetadata::load(const Config::ChunkConfig& config, const std::string& filename) {
    fs::path metadata_dir = config.getMetadataDirPath();
    fs::path metadata_path = metadata_dir / (filename + ".json");

    if (!fs::exists(metadata_path)) {
        throw std::runtime_error("Metadata file not found: " + metadata_path.string());
    }

    std::ifstream ifs(metadata_path);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open metadata file for reading: " + metadata_path.string());
    }

    nlohmann::json j;
    try {
        ifs >> j;
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error("Error parsing JSON metadata file " + metadata_path.string() + ": " + e.what());
    }
    ifs.close();

    return fromJson(j);
}

std::filesystem::path FileMetadata::getFullPath(const Config::ChunkConfig& config) const {
    return config.getMetadataDirPath() / (original_filename + ".json");
}

} // namespace Metadata
} // namespace FileManager