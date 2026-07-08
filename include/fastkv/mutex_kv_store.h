#pragma once
#include "fastkv/kv_store.h"
#include <unordered_map>
#include <shared_mutex>
#include <vector>
#include <string>

namespace fastkv {

/**
 * @brief Sharded lock-striped hash map implementation of IKVStore.
 * 
 * Divides the keyspace into separate shards, each managed by its own
 * shared_mutex, minimizing lock contention during multi-threaded access.
 */
class MutexKVStore : public IKVStore {
public:
    explicit MutexKVStore(size_t num_shards = 16);
    ~MutexKVStore() override = default;

    bool Get(const std::string& key, std::string& value) override;
    void Set(const std::string& key, const std::string& value) override;
    bool Delete(const std::string& key) override;
    void Clear() override;
    size_t Size() const override;

private:
    struct Shard {
        std::unordered_map<std::string, std::string> map;
        mutable std::shared_mutex mutex;
    };

    size_t num_shards_;
    std::vector<Shard> shards_;

    Shard& GetShard(const std::string& key);
    const Shard& GetShard(const std::string& key) const;
};

} // namespace fastkv
