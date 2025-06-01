// src/file_manager.cpp
#include "file_manager.hpp"
#include <fstream>
#include <iostream>
#include <set>       // For updateFile comparison
#include <algorithm> // For std::set_difference, std::remove

namespace fs = std::filesystem;

namespace FileManager
{

    FileManager::FileManager(size_t num_threads) : thread_pool(num_threads)
    {
        // Ensure base directories exist on startup
        config.getChunksDirPath();
        config.getMetadataDirPath();
        std::cout << "FileManager initialized." << std::endl;
    }

    // Helper to read file into chunks and generate their CIDs
    std::vector<std::string> FileManager::processFileIntoChunks(
        const std::string &filepath,
        std::vector<Chunks::Chunk> &out_chunks)
    {
        fs::path input_path(filepath);
        if (!fs::exists(input_path))
        {
            throw std::runtime_error("Input file not found: " + filepath);
        }

        std::ifstream ifs(input_path, std::ios::binary);
        if (!ifs.is_open())
        {
            throw std::runtime_error("Failed to open input file: " + filepath);
        }

        std::vector<std::string> chunk_cids;
        std::vector<std::future<std::string>> hash_futures;

        std::vector<char> buffer(Config::ChunkConfig::CHUNK_SIZE);
        size_t chunk_index = 0;

        while (ifs.read(buffer.data(), Config::ChunkConfig::CHUNK_SIZE))
        {
            // Full chunk read
            out_chunks.emplace_back(buffer);
            hash_futures.push_back(thread_pool.enqueue([&out_chunks, idx = chunk_index]()
                                                       {
                                                           return out_chunks[idx].cid; // CID already calculated in Chunk constructor
                                                       }));
            chunk_index++;
        }

        // Handle the last, partial chunk
        if (ifs.gcount() > 0)
        {
            buffer.resize(static_cast<size_t>(ifs.gcount()));
            out_chunks.emplace_back(buffer);
            hash_futures.push_back(thread_pool.enqueue([&out_chunks, idx = chunk_index]()
                                                       { return out_chunks[idx].cid; }));
            chunk_index++;
        }

        // Wait for all hashing tasks to complete and collect CIDs
        for (auto &fut : hash_futures)
        {
            chunk_cids.push_back(fut.get());
        }

        ifs.close();
        return chunk_cids;
    }

    // Corresponds to POST /files
    Metadata::FileMetadata FileManager::uploadFile(
        const std::string &input_filepath,
        const std::string &original_filename,
        const std::string &content_type)
    {
        std::cout << "Uploading file: " << original_filename << std::endl;
        std::vector<Chunks::Chunk> chunks;
        std::vector<std::string> chunk_cids;
        uint64_t file_size = fs::file_size(input_filepath);

        // Process file into chunks and get their CIDs
        chunk_cids = processFileIntoChunks(input_filepath, chunks);

        // Save unique chunks and increment reference counts
        for (const auto &chunk : chunks)
        {
            // Check if chunk exists using the reference manager or by trying to load.
            // A direct check `if (ref_manager.getCount(chunk.cid) == 0)` is efficient,
            // but `chunk.save()` also handles `fs::exists` check internally.
            // For robustness, ensure `save` only writes if not present.
            chunk.save(config); // Save handles deduplication
            ref_manager.increment(chunk.cid);
        }

        // Create and save metadata
        Metadata::FileMetadata metadata(original_filename, file_size, content_type, chunk_cids);
        metadata.save(config);

        std::cout << "File '" << original_filename << "' uploaded successfully." << std::endl;
        return metadata;
    }

    // Corresponds to GET /files/{filename}
    bool FileManager::retrieveFile(const std::string &original_filename, const std::string &output_filepath)
    {
        std::cout << "Retrieving file: " << original_filename << std::endl;
        try
        {
            Metadata::FileMetadata metadata = Metadata::FileMetadata::load(config, original_filename);

            std::ofstream ofs(output_filepath, std::ios::binary);
            if (!ofs.is_open())
            {
                throw std::runtime_error("Failed to open output file for writing: " + output_filepath);
            }

            for (const std::string &cid : metadata.chunk_cids)
            {
                std::vector<char> chunk_data = Chunks::Chunk::loadData(config, cid);
                ofs.write(chunk_data.data(), chunk_data.size());
                if (!ofs.good())
                {
                    throw std::runtime_error("Failed to write chunk data to output file during retrieval.");
                }
            }
            ofs.close();
            std::cout << "File '" << original_filename << "' retrieved to '" << output_filepath << "' successfully." << std::endl;
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error retrieving file '" << original_filename << "': " << e.what() << std::endl;
            // Clean up partially written file if error occurs
            if (fs::exists(output_filepath))
            {
                fs::remove(output_filepath);
            }
            return false;
        }
    }

    // Corresponds to GET /chunks/{hash}
    std::vector<char> FileManager::retrieveChunk(const std::string &chunk_cid)
    {
        std::cout << "Retrieving chunk: " << chunk_cid << std::endl;
        try
        {
            return Chunks::Chunk::loadData(config, chunk_cid);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error retrieving chunk '" << chunk_cid << "': " << e.what() << std::endl;
            throw; // Re-throw the exception for caller to handle
        }
    }

    // Helper to delete a chunk file if its reference count reaches zero
    bool FileManager::deleteChunkFileIfUnreferenced(const std::string &chunk_cid)
    {
        if (ref_manager.decrement(chunk_cid) == 0)
        {
            fs::path chunk_path = config.getChunksDirPath() / chunk_cid;
            if (fs::exists(chunk_path))
            {
                try
                {
                    fs::remove(chunk_path);
                    std::cout << "Deleted unreferenced chunk file: " << chunk_path << std::endl;
                    return true;
                }
                catch (const fs::filesystem_error &e)
                {
                    std::cerr << "Error deleting chunk file " << chunk_path << ": " << e.what() << std::endl;
                    return false; // Deletion failed
                }
            }
            else
            {
                // Chunk file not found, but ref count was 0. Might be an inconsistency.
                std::cerr << "Warning: Chunk file " << chunk_path << " not found, but ref count was 0. Inconsistency?" << std::endl;
                return false;
            }
        }
        return false; // Chunk not deleted because it's still referenced
    }

    // Corresponds to DELETE /files/{filename}
    bool FileManager::deleteFile(const std::string &original_filename)
    {
        std::cout << "Deleting file: " << original_filename << std::endl;
        try
        {
            Metadata::FileMetadata metadata = Metadata::FileMetadata::load(config, original_filename);

            // Decrement reference counts for all associated chunks
            // And delete chunk files if their count drops to zero
            for (const std::string &cid : metadata.chunk_cids)
            {
                deleteChunkFileIfUnreferenced(cid);
            }

            // Delete the metadata file
            fs::path metadata_path = metadata.getFullPath(config);
            if (fs::exists(metadata_path))
            {
                fs::remove(metadata_path);
                std::cout << "Deleted metadata file: " << metadata_path << std::endl;
            }
            else
            {
                std::cerr << "Warning: Metadata file for '" << original_filename << "' not found during deletion." << std::endl;
            }

            std::cout << "File '" << original_filename << "' deleted successfully." << std::endl;
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error deleting file '" << original_filename << "': " << e.what() << std::endl;
            return false;
        }
    }

    // Corresponds to PUT /files/{filename}
    Metadata::FileMetadata FileManager::updateFile(
        const std::string &original_filename,
        const std::string &updated_filepath,
        const std::string &new_content_type)
    {
        std::cout << "Updating file: " << original_filename << std::endl;
        Metadata::FileMetadata old_metadata;
        try
        {
            old_metadata = Metadata::FileMetadata::load(config, original_filename);
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error("Cannot update file: Original metadata not found for '" + original_filename + "'. " + e.what());
        }

        std::vector<Chunks::Chunk> new_file_chunks;
        std::vector<std::string> new_chunk_cids = processFileIntoChunks(updated_filepath, new_file_chunks);
        uint64_t new_file_size = fs::file_size(updated_filepath);

        // Identify chunks that are in the old file but not in the new one (to be potentially deleted)
        // and chunks that are in the new file but not in the old one (to be added/referenced).
        std::set<std::string> old_cids_set(old_metadata.chunk_cids.begin(), old_metadata.chunk_cids.end());
        std::set<std::string> new_cids_set(new_chunk_cids.begin(), new_chunk_cids.end());

        std::vector<std::string> cids_to_decrement;
        std::vector<std::string> cids_to_increment; // These are CIDs that are truly new or reused.

        // Chunks to decrement: In old set, not in new set
        std::set_difference(old_cids_set.begin(), old_cids_set.end(),
                            new_cids_set.begin(), new_cids_set.end(),
                            std::back_inserter(cids_to_decrement));

        // Chunks to increment: In new set, not in old set
        // Or, if a chunk is in *both* old and new, its reference count doesn't change relative to this file.
        // So, we only need to increment for *new* chunks.
        // The `new_file_chunks` list already contains all chunks in the new file.
        // The `uploadFile` logic (which `processFileIntoChunks` precedes) already handles saving/incrementing.
        // So, we just need to save the new chunks and increment their ref counts.

        // Save new chunks and increment their reference counts
        for (const auto &chunk : new_file_chunks)
        {
            // If the chunk is truly new (not in the old file, or just re-appearing), save and increment.
            // `chunk.save` handles if it's already on disk.
            // We always increment for chunks in the new file, then decrement for old file's chunks.
            chunk.save(config);
            ref_manager.increment(chunk.cid);
        }

        // Decrement reference counts for chunks that are no longer part of this file
        for (const std::string &cid : cids_to_decrement)
        {
            deleteChunkFileIfUnreferenced(cid);
        }

        // Create and save the updated metadata
        Metadata::FileMetadata updated_metadata(original_filename, new_file_size, new_content_type, new_chunk_cids);
        updated_metadata.save(config); // This will overwrite the old metadata file

        std::cout << "File '" << original_filename << "' updated successfully." << std::endl;
        return updated_metadata;
    }

} // namespace FileManager