#include "fastkv/slab_allocator.h"
#include <new>
#include <stdexcept>

namespace fastkv {

SlabAllocator::SlabAllocator(size_t capacity) : capacity_(capacity) {
    pool_.reserve(capacity_);
    Node* head = nullptr;
    
    for (size_t i = 0; i < capacity_; ++i) {
        Node* node = new Node();
        node->next_free = head;
        head = node;
        pool_.push_back(node);
    }
    
    free_list_head_.store(head, std::memory_order_relaxed);
}

SlabAllocator::~SlabAllocator() {
    for (Node* node : pool_) {
        delete node;
    }
}

Node* SlabAllocator::Allocate(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    Node* head = free_list_head_.load(std::memory_order_relaxed);
    if (!head) {
        throw std::bad_alloc();
    }
    
    free_list_head_.store(head->next_free, std::memory_order_relaxed);
    
    head->key = key;
    head->value = value;
    head->next.store(nullptr, std::memory_order_relaxed);
    head->next_free = nullptr;
    allocated_count_.fetch_add(1, std::memory_order_relaxed);
    return head;
}

void SlabAllocator::Deallocate(Node* node) {
    if (!node) return;
    
    // Clear dynamic string memory to prevent accumulation
    node->key.clear();
    node->value.clear();
    node->next.store(nullptr, std::memory_order_relaxed);
    
    std::lock_guard<std::mutex> lock(mutex_);
    Node* head = free_list_head_.load(std::memory_order_relaxed);
    node->next_free = head;
    free_list_head_.store(node, std::memory_order_relaxed);
    allocated_count_.fetch_sub(1, std::memory_order_relaxed);
}

} // namespace fastkv
