// src/chunk_config.cpp
#include "chunk_config.hpp"
#include <iostream>  // For error logging (can be replaced by a proper logger)
#include <stdexcept> // For std::runtime_error

namespace fs = std::filesystem;

namespace FileManager
{
    namespace Config
    {

        fs::path ChunkConfig::ensureDirectoryExists(const std::string &dir_name)
        {
            // Get the current executable's path to make directories relative to it.
            // This is a common approach for services.
            fs::path current_path = fs::current_path();
            fs::path dir_path = current_path / dir_name;

            try
            {
                if (!fs::exists(dir_path))
                {
                    if (fs::create_directories(dir_path))
                    {
                        std::cout << "Created directory: " << dir_path << std::endl;
                    }
                    else
                    {
                        // This might happen if another process created it in the meantime,
                        // or if there are permission issues.
                        // If it exists now, it's fine.
                        if (!fs::exists(dir_path))
                        {
                            throw std::runtime_error("Failed to create directory: " + dir_path.string());
                        }
                    }
                }
            }
            catch (const fs::filesystem_error &e)
            {
                throw std::runtime_error("Filesystem error creating directory " + dir_path.string() + ": " + e.what());
            }
            return dir_path;
        }

        fs::path ChunkConfig::getChunksDirPath()
        {
            return ensureDirectoryExists(CHUNKS_DIR_NAME);
        }

        fs::path ChunkConfig::getMetadataDirPath()
        {
            return ensureDirectoryExists(METADATA_DIR_NAME);
        }

    } // namespace Config
} // namespace FileManager