// include/chunk_reference_manager.hpp
#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <memory> // For std::shared_ptr

namespace FileManager
{
    namespace Chunks
    {

        // ChunkReferenceManager will manage reference counts for chunks.
        // For a production system, this state would typically be persisted
        // (e.g., to a database or a dedicated reference file) to survive service restarts.
        // For this project, we'll keep it in-memory as a starting point.
        class ChunkReferenceManager
        {
        public:
            ChunkReferenceManager();

            // Increment the reference count for a given chunk CID.
            void increment(const std::string &chunk_cid);

            // Decrement the reference count for a given chunk CID.
            // Returns the new count. If 0, the chunk can be considered for deletion.
            int decrement(const std::string &chunk_cid);

            // Get the current reference count for a given chunk CID.
            int getCount(const std::string &chunk_cid) const;

        private:
            // Map to store chunk CID to its reference count
            std::unordered_map<std::string, int> reference_counts;
            mutable std::mutex mtx; // Mutex for thread-safe access to reference_counts
        };

    } // namespace Chunks
} // namespace FileManager