#pragma once
#include "fastkv/node.h"
#include "fastkv/slab_allocator.h"
#include <atomic>
#include <vector>
#include <cstddef>

namespace fastkv {

constexpr size_t MAX_THREADS = 128;
constexpr size_t RETIRED_THRESHOLD = 64;

/**
 * @brief Thread-safe Hazard Pointer Registry for safe memory reclamation.
 * 
 * Permits reading threads to protect nodes they are currently accessing
 * from being deleted by concurrently deleting threads.
 */
class HazardPointerRegistry {
public:
    static HazardPointerRegistry& GetInstance();

    // Acquire registry record for the current thread
    void Protect(size_t slot, Node* node);
    void Unprotect(size_t slot);

    /**
     * @brief Defers deallocation of a node until it is no longer referenced by any thread.
     * @param node The unlinked node.
     * @param allocator The allocator to return the node to once safe.
     */
    void Retire(Node* node, SlabAllocator& allocator);

    // Forces a garbage collection sweep of the current thread's retired nodes
    void ForceReclaim(SlabAllocator& allocator);

    struct ThreadRecord {
        std::atomic<Node*> hp[2]; // Supporting up to 2 protected nodes per thread
        std::atomic<bool> active{false};
    };

private:
    HazardPointerRegistry();
    ~HazardPointerRegistry();

    // Prevent copying
    HazardPointerRegistry(const HazardPointerRegistry&) = delete;
    HazardPointerRegistry& operator=(const HazardPointerRegistry&) = delete;

    ThreadRecord records_[MAX_THREADS];

    ThreadRecord* GetThreadRecord();

    // Static thread-local parameters
    static thread_local std::vector<Node*> retired_list_;
    static thread_local ThreadRecord* my_record_;

    void Scan(SlabAllocator& allocator);
};

} // namespace fastkv
