#pragma once
#include <string>
#include <optional>
#include <cstddef>

namespace fastkv {

/**
 * @brief Thread-safe and swappable interface for FastKV backends.
 * 
 * Both the baseline mutex-based store and the lock-free hazard-pointer-based
 * store implement this interface to allow seamless benchmarking and swaps.
 */
class IKVStore {
public:
    virtual ~IKVStore() = default;

    /**
     * @brief Retrieves the value associated with a key.
     * 
     * @param key The key to look up.
     * @param value Output reference where the found value will be stored.
     * @return true If the key was found.
     * @return false If the key was not found.
     * 
     * @note In multi-threaded backends, this must support concurrent readers.
     */
    virtual bool Get(const std::string& key, std::string& value) = 0;

    /**
     * @brief Inserts or updates a key-value pair.
     * 
     * @param key The key to insert or update.
     * @param value The value associated with the key.
     */
    virtual void Set(const std::string& key, const std::string& value) = 0;

    /**
     * @brief Deletes a key-value pair from the store.
     * 
     * @param key The key to delete.
     * @return true If the key existed and was successfully deleted.
     * @return false If the key did not exist.
     */
    virtual bool Delete(const std::string& key) = 0;

    /**
     * @brief Removes all keys and values from the store.
     */
    virtual void Clear() = 0;

    /**
     * @brief Returns the total number of items currently in the store.
     * 
     * @return size_t Current item count.
     */
    virtual size_t Size() const = 0;
};

} // namespace fastkv
