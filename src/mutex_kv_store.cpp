#include "fastkv/mutex_kv_store.h"

namespace fastkv {

MutexKVStore::MutexKVStore(size_t num_shards) : num_shards_(num_shards), shards_(num_shards) {}

MutexKVStore::Shard& MutexKVStore::GetShard(const std::string& key) {
    size_t hash = std::hash<std::string>{}(key);
    return shards_[hash % num_shards_];
}

const MutexKVStore::Shard& MutexKVStore::GetShard(const std::string& key) const {
    size_t hash = std::hash<std::string>{}(key);
    return shards_[hash % num_shards_];
}

bool MutexKVStore::Get(const std::string& key, std::string& value) {
    const Shard& shard = GetShard(key);
    std::shared_lock<std::shared_mutex> lock(shard.mutex);
    
    auto it = shard.map.find(key);
    if (it == shard.map.end()) {
        return false;
    }
    value = it->second;
    return true;
}

void MutexKVStore::Set(const std::string& key, const std::string& value) {
    Shard& shard = GetShard(key);
    std::unique_lock<std::shared_mutex> lock(shard.mutex);
    shard.map[key] = value;
}

bool MutexKVStore::Delete(const std::string& key) {
    Shard& shard = GetShard(key);
    std::unique_lock<std::shared_mutex> lock(shard.mutex);
    return shard.map.erase(key) > 0;
}

void MutexKVStore::Clear() {
    for (size_t i = 0; i < num_shards_; ++i) {
        std::unique_lock<std::shared_mutex> lock(shards_[i].mutex);
        shards_[i].map.clear();
    }
}

size_t MutexKVStore::Size() const {
    size_t total = 0;
    for (size_t i = 0; i < num_shards_; ++i) {
        std::shared_lock<std::shared_mutex> lock(shards_[i].mutex);
        total += shards_[i].map.size();
    }
    return total;
}

} // namespace fastkv
