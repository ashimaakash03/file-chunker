// include/file_manager.hpp
#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <stdexcept> // For std::runtime_error

#include "chunk_config.hpp"
#include "cid_utility.hpp"
#include "chunk.hpp"
#include "file_metadata.hpp"
#include "chunk_reference_manager.hpp"
#include "thread_pool.hpp"

namespace FileManager
{

    class FileManager
    {
    public:
        // Constructor
        FileManager(size_t num_threads);

        // --- API Endpoints/Functionalities as per PRD ---

        // Corresponds to POST /files
        // Uploads a file, chunks it, stores chunks, and creates metadata.
        Metadata::FileMetadata uploadFile(const std::string &input_filepath,
                                          const std::string &original_filename,
                                          const std::string &content_type);

        // Corresponds to GET /files/{filename}
        // Retrieves a file by reassembling its chunks.
        bool retrieveFile(const std::string &original_filename, const std::string &output_filepath);

        // Corresponds to GET /chunks/{hash}
        // Retrieves a specific chunk by its CID (hash).
        std::vector<char> retrieveChunk(const std::string &chunk_cid);

        // Corresponds to DELETE /files/{filename}
        // Deletes a file and its associated chunks if no other files reference them.
        bool deleteFile(const std::string &original_filename);

        // Corresponds to PUT /files/{filename}
        // Updates an existing file with new content. Handles chunk diffing and updates.
        Metadata::FileMetadata updateFile(const std::string &original_filename,
                                          const std::string &updated_filepath,
                                          const std::string &new_content_type);

    private:
        Config::ChunkConfig config;
        CID::CIDUtility cid_utility; // Static class, but good to have
        Chunks::ChunkReferenceManager ref_manager;
        Concurrency::ThreadPool thread_pool;

        // Helper to read file into chunks and generate their CIDs
        std::vector<std::string> processFileIntoChunks(const std::string &filepath,
                                                       std::vector<Chunks::Chunk> &out_chunks);

        // Helper to delete a chunk file if its reference count reaches zero
        bool deleteChunkFileIfUnreferenced(const std::string &chunk_cid);
    };

} // namespace FileManager