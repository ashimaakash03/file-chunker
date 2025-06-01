// main.cpp
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <chrono> // For timing operations

#include "file_manager.hpp"
#include "chunk_config.hpp" // For CHUNKS_DIR_NAME, METADATA_DIR_NAME

namespace fs = std::filesystem;

// Helper to create a dummy file for testing
void createDummyFile(const std::string &filename, size_t size_mb)
{
    fs::path filepath = fs::current_path() / filename;
    std::ofstream ofs(filepath, std::ios::binary);
    if (!ofs.is_open())
    {
        std::cerr << "Failed to create dummy file: " << filepath << std::endl;
        return;
    }
    const size_t buffer_size = 1024 * 1024; // 1MB
    std::vector<char> buffer(buffer_size);
    for (size_t i = 0; i < size_mb; ++i)
    {
        // Fill buffer with some unique data for each MB (e.g., repeating char 'A' + i)
        std::fill(buffer.begin(), buffer.end(), static_cast<char>('A' + (i % 26)));
        ofs.write(buffer.data(), buffer.size());
    }
    ofs.close();
    std::cout << "Created dummy file: " << filename << " (" << size_mb << " MB)" << std::endl;
}

// Helper to compare two files
bool compareFiles(const std::string &file1, const std::string &file2)
{
    std::ifstream f1(file1, std::ios::binary | std::ios::ate);
    std::ifstream f2(file2, std::ios::binary | std::ios::ate);

    if (!f1.is_open() || !f2.is_open())
    {
        std::cerr << "Error: Could not open one of the files for comparison." << std::endl;
        return false;
    }

    if (f1.tellg() != f2.tellg())
    {
        return false; // Sizes differ
    }

    f1.seekg(0, std::ios::beg);
    f2.seekg(0, std::ios::beg);

    return std::equal(std::istreambuf_iterator<char>(f1),
                      std::istreambuf_iterator<char>(),
                      std::istreambuf_iterator<char>(f2));
}

int main()
{
    std::cout << "--- File Manager Service Demo ---" << std::endl;

    // Use a fixed number of threads for the thread pool
    // Adjust based on your system's core count
    const size_t num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0)
    {
        std::cerr << "Warning: Could not determine hardware concurrency. Using 4 threads." << std::endl;
        // Fallback to a default reasonable number
        FileManager::FileManager fm(4);
    }

    FileManager::FileManager fm(num_threads);

    // Clean up previous test runs (optional, but good for consistent testing)
    try
    {
        if (fs::exists(FileManager::Config::ChunkConfig::CHUNKS_DIR_NAME))
        {
            fs::remove_all(FileManager::Config::ChunkConfig::CHUNKS_DIR_NAME);
            std::cout << "Cleaned up existing chunks directory." << std::endl;
        }
        if (fs::exists(FileManager::Config::ChunkConfig::METADATA_DIR_NAME))
        {
            fs::remove_all(FileManager::Config::ChunkConfig::METADATA_DIR_NAME);
            std::cout << "Cleaned up existing metadata directory." << std::endl;
        }
        // Ensure directories are re-created by FileManager constructor/first use
        FileManager::Config::ChunkConfig::getChunksDirPath();
        FileManager::Config::ChunkConfig::getMetadataDirPath();
    }
    catch (const fs::filesystem_error &e)
    {
        std::cerr << "Error during cleanup: " << e.what() << std::endl;
        return 1;
    }

    std::string file1_name = "document_5MB.txt";
    std::string file2_name = "image_10MB.bin";
    std::string file3_name = "duplicate_5MB.txt";        // Same content as file1_name
    std::string file4_name = "updated_document_5MB.txt"; // Slightly modified file1

    // --- 1. Upload a file ---
    std::cout << "\n--- Uploading " << file1_name << " (5 MB) ---" << std::endl;
    createDummyFile(file1_name, 5);
    try
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        FileManager::Metadata::FileMetadata meta1 = fm.uploadFile(file1_name, file1_name, "text/plain");
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end_time - start_time;
        std::cout << "Upload completed in " << duration.count() << " seconds." << std::endl;

        std::cout << "Uploaded file metadata:" << std::endl;
        std::cout << "  Filename: " << meta1.original_filename << std::endl;
        std::cout << "  Size: " << meta1.file_size_bytes << " bytes" << std::endl;
        std::cout << "  Chunks: " << meta1.chunk_cids.size() << std::endl;
        std::cout << "  First chunk CID: " << meta1.chunk_cids[0] << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during upload: " << e.what() << std::endl;
    }

    // --- 2. Upload another file (different content) ---
    std::cout << "\n--- Uploading " << file2_name << " (10 MB) ---" << std::endl;
    createDummyFile(file2_name, 10);
    try
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        FileManager::Metadata::FileMetadata meta2 = fm.uploadFile(file2_name, file2_name, "application/octet-stream");
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end_time - start_time;
        std::cout << "Upload completed in " << duration.count() << " seconds." << std::endl;
        std::cout << "Uploaded file metadata:" << std::endl;
        std::cout << "  Filename: " << meta2.original_filename << std::endl;
        std::cout << "  Size: " << meta2.file_size_bytes << " bytes" << std::endl;
        std::cout << "  Chunks: " << meta2.chunk_cids.size() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during upload: " << e.what() << std::endl;
    }

    // --- 3. Upload a duplicate file to test deduplication ---
    std::cout << "\n--- Uploading " << file3_name << " (5 MB - duplicate of " << file1_name << ") ---" << std::endl;
    createDummyFile(file3_name, 5); // Should have same content as file1_name
    try
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        FileManager::Metadata::FileMetadata meta3 = fm.uploadFile(file3_name, file3_name, "text/plain");
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end_time - start_time;
        std::cout << "Upload completed in " << duration.count() << " seconds." << std::endl;
        std::cout << "Uploaded file metadata:" << std::endl;
        std::cout << "  Filename: " << meta3.original_filename << std::endl;
        std::cout << "  Size: " << meta3.file_size_bytes << " bytes" << std::endl;
        std::cout << "  Chunks: " << meta3.chunk_cids.size() << std::endl;
        std::cout << "  First chunk CID: " << meta3.chunk_cids[0] << std::endl;
        // Verify CIDs are the same
        FileManager::Metadata::FileMetadata original_meta1 = FileManager::Metadata::FileMetadata::load(FileManager::Config::ChunkConfig(), file1_name);
        if (meta3.chunk_cids[0] == original_meta1.chunk_cids[0])
        {
            std::cout << "Deduplication test PASSED: First chunk CIDs match for " << file1_name << " and " << file3_name << std::endl;
        }
        else
        {
            std::cout << "Deduplication test FAILED: First chunk CIDs do NOT match." << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during duplicate upload: " << e.what() << std::endl;
    }

    // --- 4. Retrieve a file ---
    std::string retrieved_file1_name = "retrieved_" + file1_name;
    std::cout << "\n--- Retrieving " << file1_name << " ---" << std::endl;
    try
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        bool success = fm.retrieveFile(file1_name, retrieved_file1_name);
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end_time - start_time;
        std::cout << "Retrieval completed in " << duration.count() << " seconds." << std::endl;

        if (success)
        {
            std::cout << "File retrieved successfully to: " << retrieved_file1_name << std::endl;
            if (compareFiles(file1_name, retrieved_file1_name))
            {
                std::cout << "File integrity check PASSED." << std::endl;
            }
            else
            {
                std::cout << "File integrity check FAILED: Retrieved file differs from original." << std::endl;
            }
        }
        else
        {
            std::cout << "File retrieval failed." << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during retrieval: " << e.what() << std::endl;
    }

    // --- 5. Retrieve a specific chunk ---
    std::cout << "\n--- Retrieving a specific chunk from " << file1_name << " ---" << std::endl;
    try
    {
        FileManager::Metadata::FileMetadata meta1_retrieved = FileManager::Metadata::FileMetadata::load(FileManager::Config::ChunkConfig(), file1_name);
        if (!meta1_retrieved.chunk_cids.empty())
        {
            std::string first_chunk_cid = meta1_retrieved.chunk_cids[0];
            std::vector<char> chunk_data = fm.retrieveChunk(first_chunk_cid);
            std::cout << "Retrieved chunk with CID: " << first_chunk_cid << ", size: " << chunk_data.size() << " bytes." << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error retrieving chunk: " << e.what() << std::endl;
    }

    // --- 6. Update a file ---
    std::cout << "\n--- Updating " << file1_name << " with " << file4_name << " ---" << std::endl;
    // Create an updated version of file1_name. Let's make it 6MB for example.
    createDummyFile(file4_name, 6);
    try
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        FileManager::Metadata::FileMetadata updated_meta = fm.updateFile(file1_name, file4_name, "text/markdown");
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end_time - start_time;
        std::cout << "Update completed in " << duration.count() << " seconds." << std::endl;

        std::cout << "Updated file metadata for " << original_filename << ":" << std::endl;
        std::cout << "  New size: " << updated_meta.file_size_bytes << " bytes" << std::endl;
        std::cout << "  New chunk count: " << updated_meta.chunk_cids.size() << std::endl;
        std::cout << "  New content type: " << updated_meta.content_type << std::endl;

        // Verify retrieval of updated file
        std::string retrieved_updated_file1 = "retrieved_updated_" + file1_name;
        fm.retrieveFile(file1_name, retrieved_updated_file1);
        if (compareFiles(file4_name, retrieved_updated_file1))
        {
            std::cout << "Updated file integrity check PASSED." << std::endl;
        }
        else
        {
            std::cout << "Updated file integrity check FAILED." << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during file update: " << e.what() << std::endl;
    }

    // --- 7. Delete a file (that is duplicated) ---
    std::cout << "\n--- Deleting " << file3_name << " (which is a duplicate of " << file1_name << ") ---" << std::endl;
    try
    {
        bool deleted_duplicate = fm.deleteFile(file3_name);
        if (deleted_duplicate)
        {
            std::cout << file3_name << " deleted successfully." << std::endl;
            // The chunks of file3_name should NOT be deleted if file1_name still references them.
            // We can check if the first chunk of file1_name still exists.
            FileManager::Metadata::FileMetadata meta1_post_delete = FileManager::Metadata::FileMetadata::load(FileManager::Config::ChunkConfig(), file1_name);
            fs::path first_chunk_path = FileManager::Config::ChunkConfig::getChunksDirPath() / meta1_post_delete.chunk_cids[0];
            if (fs::exists(first_chunk_path))
            {
                std::cout << "Deduplication delete test PASSED: Chunk '" << meta1_post_delete.chunk_cids[0] << "' still exists as expected." << std::endl;
            }
            else
            {
                std::cout << "Deduplication delete test FAILED: Chunk '" << meta1_post_delete.chunk_cids[0] << "' was unexpectedly deleted." << std::endl;
            }
        }
        else
        {
            std::cout << file3_name << " deletion failed." << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during duplicate file deletion: " << e.what() << std::endl;
    }

    // --- 8. Delete the last file (its chunks should now be deleted) ---
    std::cout << "\n--- Deleting " << file1_name << " (its chunks should now be deleted if unreferenced) ---" << std::endl;
    try
    {
        // Load meta for file1_name to get its chunk CIDs before deletion
        FileManager::Metadata::FileMetadata meta1_final_delete = FileManager::Metadata::FileMetadata::load(FileManager::Config::ChunkConfig(), file1_name);
        std::string first_chunk_cid = meta1_final_delete.chunk_cids[0];
        fs::path first_chunk_path = FileManager::Config::ChunkConfig::getChunksDirPath() / first_chunk_cid;

        bool deleted_file1 = fm.deleteFile(file1_name);
        if (deleted_file1)
        {
            std::cout << file1_name << " deleted successfully." << std::endl;
            // Now, the chunks of file1_name should be gone.
            if (!fs::exists(first_chunk_path))
            {
                std::cout << "Final delete test PASSED: Chunk '" << first_chunk_cid << "' is correctly deleted." << std::endl;
            }
            else
            {
                std::cout << "Final delete test FAILED: Chunk '" << first_chunk_cid << "' was NOT deleted." << std::endl;
            }
        }
        else
        {
            std::cout << file1_name << " deletion failed." << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during final file deletion: " << e.what() << std::endl;
    }

    // --- Cleanup dummy files created for testing ---
    std::cout << "\n--- Cleaning up dummy test files ---" << std::endl;
    fs::remove(file1_name);
    fs::remove(file2_name);
    fs::remove(file3_name);
    fs::remove(file4_name);
    fs::remove(retrieved_file1_name);
    fs::remove(retrieved_updated_file1);
    std::cout << "Dummy test files removed." << std::endl;

    std::cout << "\n--- Demo End ---" << std::endl;

    return 0;
}