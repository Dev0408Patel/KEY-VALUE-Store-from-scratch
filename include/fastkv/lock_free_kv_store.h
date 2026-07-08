#pragma once
#include "fastkv/kv_store.h"
#include "fastkv/node.h"
#include "fastkv/slab_allocator.h"
#include <atomic>
#include <string>

namespace fastkv {

/**
 * @brief Lock-free hash map implementation of IKVStore.
 * 
 * Uses atomic CAS operations for pointer updates, a custom Slab Allocator
 * for node pooling, and the HazardPointerRegistry for safe memory reclamation.
 */
class LockFreeKVStore : public IKVStore {
public:
    explicit LockFreeKVStore(size_t num_buckets = 4096);
    ~LockFreeKVStore() override;

    bool Get(const std::string& key, std::string& value) override;
    void Set(const std::string& key, const std::string& value) override;
    bool Delete(const std::string& key) override;
    void Clear() override;
    size_t Size() const override;

private:
    size_t num_buckets_;
    std::atomic<Node*>* buckets_;
    SlabAllocator allocator_;
    std::atomic<size_t> size_{0};
};

} // namespace fastkv
