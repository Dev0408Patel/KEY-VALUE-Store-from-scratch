#pragma once
#include "fastkv/node.h"
#include <vector>
#include <atomic>
#include <cstddef>
#include <mutex>

namespace fastkv {

/**
 * @brief Thread-safe lock-free memory allocator for Node objects.
 * 
 * Pre-allocates a pool of Node memory blocks during initialization,
 * avoiding dynamic heap allocation (malloc/new) on the hot database path.
 */
class SlabAllocator {
public:
    explicit SlabAllocator(size_t capacity = 50000);
    ~SlabAllocator();

    // Prevent copying to avoid double frees of pre-allocated slabs
    SlabAllocator(const SlabAllocator&) = delete;
    SlabAllocator& operator=(const SlabAllocator&) = delete;

    /**
     * @brief Allocates a Node block.
     * @return Node* Pointer to the allocated Node.
     * @throw std::bad_alloc If the allocator pool is fully exhausted.
     */
    Node* Allocate(const std::string& key, const std::string& value);

    /**
     * @brief Reclaims a Node block and returns it to the free pool.
     * @param node The node to deallocate.
     */
    void Deallocate(Node* node);

    size_t AllocatedCount() const { return allocated_count_.load(); }
    size_t Capacity() const { return capacity_; }

private:
    size_t capacity_;
    std::vector<Node*> pool_; // Owns the underlying nodes
    std::atomic<Node*> free_list_head_{nullptr};
    std::atomic<size_t> allocated_count_{0};
    mutable std::mutex mutex_;
};

} // namespace fastkv
