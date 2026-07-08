#pragma once
#include <string>
#include <atomic>

namespace fastkv {

/**
 * @brief Node representation in FastKV backends.
 */
struct Node {
    std::string key;
    std::string value;
    std::atomic<Node*> next{nullptr};
    
    // Auxiliary pointer for free list linking in the Slab Allocator
    Node* next_free{nullptr};

    Node() = default;
    Node(const std::string& k, const std::string& v) : key(k), value(v) {}
};

} // namespace fastkv
