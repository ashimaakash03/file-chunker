// include/chunk_config.hpp
#pragma once

#include <string>
#include <cstddef>    // For size_t
#include <filesystem> // For std::filesystem::path

namespace FileManager
{
    namespace Config
    {

        class ChunkConfig
        {
        public:
            // Define the size of each chunk (1MB)
            static const size_t CHUNK_SIZE = 1024 * 1024;

            // Define the names of the directories for chunks and metadata
            static const std::string CHUNKS_DIR_NAME;
            static const std::string METADATA_DIR_NAME;

            // Get the absolute path for the chunks directory
            // This will create the directory if it doesn't exist
            static std::filesystem::path getChunksDirPath();

            // Get the absolute path for the metadata directory
            // This will create the directory if it doesn't exist
            static std::filesystem::path getMetadataDirPath();

        private:
            // Helper to ensure directories exist
            static std::filesystem::path ensureDirectoryExists(const std::string &dir_name);
        };

        // Initialize static members
        // These are defined in the .cpp file, but declared here.
        // For simple static const string members, they can be defined right here if preferred.
        // Let's define them here for simplicity as they are compile-time constants.
        const std::string ChunkConfig::CHUNKS_DIR_NAME = "chunks";
        const std::string ChunkConfig::METADATA_DIR_NAME = "metadata";

    } // namespace Config
} // namespace FileManager