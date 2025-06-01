// src/chunk_reference_manager.cpp
#include "chunk_reference_manager.hpp"
#include <iostream> // For logging

namespace FileManager
{
    namespace Chunks
    {

        ChunkReferenceManager::ChunkReferenceManager()
        {
            std::cout << "ChunkReferenceManager initialized." << std::endl;
            // In a real system, you might load saved counts from a persistent store here.
        }

        void ChunkReferenceManager::increment(const std::string &chunk_cid)
        {
            std::lock_guard<std::mutex> lock(mtx);
            reference_counts[chunk_cid]++;
            // std::cout << "Incremented ref count for " << chunk_cid << ". New count: " << reference_counts[chunk_cid] << std::endl;
        }

        int ChunkReferenceManager::decrement(const std::string &chunk_cid)
        {
            std::lock_guard<std::mutex> lock(mtx);
            auto it = reference_counts.find(chunk_cid);
            if (it == reference_counts.end() || it->second <= 0)
            {
                // This indicates an error state or a bug in logic
                // std::cerr << "Warning: Attempted to decrement non-existent or zero-count chunk: " << chunk_cid << std::endl;
                return 0; // Or throw an error depending on desired strictness
            }
            it->second--;
            // std::cout << "Decremented ref count for " << chunk_cid << ". New count: " << it->second << std::endl;
            return it->second;
        }

        int ChunkReferenceManager::getCount(const std::string &chunk_cid) const
        {
            std::lock_guard<std::mutex> lock(mtx);
            auto it = reference_counts.find(chunk_cid);
            if (it == reference_counts.end())
            {
                return 0;
            }
            return it->second;
        }

    } // namespace Chunks
} // namespace FileManager