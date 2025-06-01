// src/chunk.cpp
#include "chunk.hpp"
#include <fstream>
#include <iostream> // For logging

namespace fs = std::filesystem;

namespace FileManager
{
    namespace Chunks
    {

        bool Chunk::save(const Config::ChunkConfig &config)
        {
            fs::path chunk_dir = config.getChunksDirPath();
            fs::path chunk_path = chunk_dir / cid;

            if (fs::exists(chunk_path))
            {
                // Chunk already exists (deduplication)
                // std::cout << "Chunk already exists, skipping save: " << cid << std::endl;
                return true;
            }

            std::ofstream ofs(chunk_path, std::ios::binary);
            if (!ofs.is_open())
            {
                throw std::runtime_error("Failed to open file for writing chunk: " + chunk_path.string());
            }
            ofs.write(data.data(), data.size());
            if (!ofs.good())
            {
                throw std::runtime_error("Failed to write all data to chunk file: " + chunk_path.string());
            }
            ofs.close();
            // std::cout << "Chunk saved: " << cid << std::endl;
            return true;
        }

        std::vector<char> Chunk::loadData(const Config::ChunkConfig &config, const std::string &chunk_cid)
        {
            fs::path chunk_dir = config.getChunksDirPath();
            fs::path chunk_path = chunk_dir / chunk_cid;

            if (!fs::exists(chunk_path))
            {
                throw std::runtime_error("Chunk file not found: " + chunk_path.string());
            }

            std::ifstream ifs(chunk_path, std::ios::binary | std::ios::ate);
            if (!ifs.is_open())
            {
                throw std::runtime_error("Failed to open chunk file for reading: " + chunk_path.string());
            }

            std::streamsize size = ifs.tellg();
            if (size == -1)
            {
                throw std::runtime_error("Failed to get size of chunk file: " + chunk_path.string());
            }
            ifs.seekg(0, std::ios::beg);

            std::vector<char> buffer(static_cast<size_t>(size));
            if (!ifs.read(buffer.data(), size))
            {
                throw std::runtime_error("Failed to read all data from chunk file: " + chunk_path.string());
            }
            ifs.close();
            return buffer;
        }

        std::filesystem::path Chunk::getFullPath(const Config::ChunkConfig &config) const
        {
            return config.getChunksDirPath() / cid;
        }

    } // namespace Chunks
} // namespace FileManager