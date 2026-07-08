#include <gtest/gtest.h>
#include <vector>
#include <thread>
#include <string>
#include <unordered_map>
#include "fastkv/kv_store.h"
#include "fastkv/mutex_kv_store.h"
#include "fastkv/lock_free_kv_store.h"

namespace fastkv {

// A trivial, single-threaded implementation of IKVStore for interface verification.
class TrivialKVStore : public IKVStore {
public:
    bool Get(const std::string& key, std::string& value) override {
        auto it = map_.find(key);
        if (it == map_.end()) {
            return false;
        }
        value = it->second;
        return true;
    }

    void Set(const std::string& key, const std::string& value) override {
        map_[key] = value;
    }

    bool Delete(const std::string& key) override {
        return map_.erase(key) > 0;
    }

    void Clear() override {
        map_.clear();
    }

    size_t Size() const override {
        return map_.size();
    }

private:
    std::unordered_map<std::string, std::string> map_;
};

// Generic correctness test runner
template <typename T>
void VerifyBasicOperations() {
    T store;
    
    // Verify initial size
    EXPECT_EQ(store.Size(), 0U);
    
    // Verify insert/retrieve
    store.Set("key1", "value1");
    EXPECT_EQ(store.Size(), 1U);
    
    std::string out;
    EXPECT_TRUE(store.Get("key1", out));
    EXPECT_EQ(out, "value1");
    
    // Verify overwrite
    store.Set("key1", "value2");
    EXPECT_EQ(store.Size(), 1U);
    EXPECT_TRUE(store.Get("key1", out));
    EXPECT_EQ(out, "value2");
    
    // Verify missing key lookup
    EXPECT_FALSE(store.Get("key2", out));
    
    // Verify deletion
    EXPECT_TRUE(store.Delete("key1"));
    EXPECT_EQ(store.Size(), 0U);
    EXPECT_FALSE(store.Get("key1", out));
    
    // Verify redundant deletion
    EXPECT_FALSE(store.Delete("key1"));
}

template <typename T>
void VerifyClearOperation() {
    T store;
    store.Set("k1", "v1");
    store.Set("k2", "v2");
    EXPECT_EQ(store.Size(), 2U);
    
    store.Clear();
    EXPECT_EQ(store.Size(), 0U);
}

// Multi-threaded stress test runner
void RunStressTest(fastkv::IKVStore& store) {
    const int num_writers = 6;
    const int num_readers = 6;
    const int num_deleters = 2;
    const int ops_per_thread = 1000;

    std::vector<std::thread> threads;

    // Writers: Add and update keys
    for (int i = 0; i < num_writers; ++i) {
        threads.emplace_back([&store, i]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                std::string key = "key_" + std::to_string(i) + "_" + std::to_string(j);
                std::string val = "value_" + std::to_string(j);
                store.Set(key, val);
            }
        });
    }

    // Readers: Get keys concurrently
    for (int i = 0; i < num_readers; ++i) {
        threads.emplace_back([&store]() {
            std::string val;
            for (int j = 0; j < ops_per_thread; ++j) {
                // Read from writer 0's key range
                std::string key = "key_0_" + std::to_string(j);
                store.Get(key, val);
            }
        });
    }

    // Deleters: Delete keys concurrently
    for (int i = 0; i < num_deleters; ++i) {
        threads.emplace_back([&store, i]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                std::string key = "key_" + std::to_string(i) + "_" + std::to_string(j);
                store.Delete(key);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Post-stress validation: Ensure it responds to Clear cleanly
    store.Clear();
    EXPECT_EQ(store.Size(), 0U);
}

} // namespace fastkv

// TrivialKVStore Tests
TEST(TrivialKVStoreTest, BasicOperations) {
    fastkv::VerifyBasicOperations<fastkv::TrivialKVStore>();
}

TEST(TrivialKVStoreTest, ClearOperation) {
    fastkv::VerifyClearOperation<fastkv::TrivialKVStore>();
}

// MutexKVStore Tests
TEST(MutexKVStoreTest, BasicOperations) {
    fastkv::VerifyBasicOperations<fastkv::MutexKVStore>();
}

TEST(MutexKVStoreTest, ClearOperation) {
    fastkv::VerifyClearOperation<fastkv::MutexKVStore>();
}

TEST(MutexKVStoreTest, MultiThreadedStress) {
    fastkv::MutexKVStore store;
    fastkv::RunStressTest(store);
}

// LockFreeKVStore Tests
TEST(LockFreeKVStoreTest, BasicOperations) {
    fastkv::VerifyBasicOperations<fastkv::LockFreeKVStore>();
}

TEST(LockFreeKVStoreTest, ClearOperation) {
    fastkv::VerifyClearOperation<fastkv::LockFreeKVStore>();
}

TEST(LockFreeKVStoreTest, MultiThreadedStress) {
    fastkv::LockFreeKVStore store;
    fastkv::RunStressTest(store);
}
