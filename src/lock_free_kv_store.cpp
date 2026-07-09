#include "fastkv/lock_free_kv_store.h"
#include "fastkv/hazard_pointer.h"

namespace fastkv {

LockFreeKVStore::LockFreeKVStore(size_t num_buckets)
    : num_buckets_(num_buckets), allocator_(50000) {
  buckets_ = new std::atomic<Node *>[num_buckets_];
  for (size_t i = 0; i < num_buckets_; ++i) {
    buckets_[i].store(nullptr, std::memory_order_relaxed);
  }
}

LockFreeKVStore::~LockFreeKVStore() {
  Clear();
  delete[] buckets_;
}

bool LockFreeKVStore::Get(const std::string &key, std::string &value) {
  size_t bucket_idx = std::hash<std::string>{}(key) % num_buckets_;

  Node *curr = buckets_[bucket_idx].load(std::memory_order_seq_cst);
  Node *prev = nullptr;

  while (curr) {
    // Protect current node in HP slot 0
    HazardPointerRegistry::GetInstance().Protect(0, curr);

    // Double-check visibility
    if (prev) {
      if (prev->next.load(std::memory_order_seq_cst) != curr) {
        // List structure updated, restart search
        curr = buckets_[bucket_idx].load(std::memory_order_seq_cst);
        prev = nullptr;
        continue;
      }
    } else {
      if (buckets_[bucket_idx].load(std::memory_order_seq_cst) != curr) {
        curr = buckets_[bucket_idx].load(std::memory_order_seq_cst);
        continue;
      }
    }

    if (curr->key == key) {
      value = curr->value;
      HazardPointerRegistry::GetInstance().Unprotect(0);
      HazardPointerRegistry::GetInstance().Unprotect(1);
      return true;
    }

    prev = curr;
    // Protect next node in HP slot 1
    Node *next = curr->next.load(std::memory_order_seq_cst);
    HazardPointerRegistry::GetInstance().Protect(1, next);
    curr = next;

    // Swap protections: move next to slot 0 and unprotect slot 1
    HazardPointerRegistry::GetInstance().Protect(0, curr);
    HazardPointerRegistry::GetInstance().Unprotect(1);
  }

  HazardPointerRegistry::GetInstance().Unprotect(0);
  return false;
}

void LockFreeKVStore::Set(const std::string &key, const std::string &value) {
  size_t bucket_idx = std::hash<std::string>{}(key) % num_buckets_;

  while (true) {
    Node *head = buckets_[bucket_idx].load(std::memory_order_seq_cst);
    Node *prev = nullptr;
    Node *curr = head;
    Node *target = nullptr;

    while (curr) {
      if (curr->key == key) {
        target = curr;
        break;
      }
      prev = curr;
      curr = curr->next.load(std::memory_order_seq_cst);
    }

    Node *new_node = allocator_.Allocate(key, value);

    if (target) {
      // Replace target node
      Node *next_node = target->next.load(std::memory_order_seq_cst);
      new_node->next.store(next_node, std::memory_order_relaxed);

      if (prev) {
        if (prev->next.compare_exchange_weak(target, new_node,
                                             std::memory_order_seq_cst,
                                             std::memory_order_seq_cst)) {
          HazardPointerRegistry::GetInstance().Retire(target, allocator_);
          break;
        } else {
          allocator_.Deallocate(new_node);
        }
      } else {
        if (buckets_[bucket_idx].compare_exchange_weak(
                head, new_node, std::memory_order_seq_cst,
                std::memory_order_seq_cst)) {
          HazardPointerRegistry::GetInstance().Retire(target, allocator_);
          break;
        } else {
          allocator_.Deallocate(new_node);
        }
      }
    } else {
      // Insert at head
      new_node->next.store(head, std::memory_order_relaxed);
      if (buckets_[bucket_idx].compare_exchange_weak(
              head, new_node, std::memory_order_seq_cst,
              std::memory_order_seq_cst)) {
        size_.fetch_add(1, std::memory_order_relaxed);
        break;
      } else {
        allocator_.Deallocate(new_node);
      }
    }
  }
}

bool LockFreeKVStore::Delete(const std::string &key) {
  size_t bucket_idx = std::hash<std::string>{}(key) % num_buckets_;

  while (true) {
    Node *head = buckets_[bucket_idx].load(std::memory_order_seq_cst);
    Node *prev = nullptr;
    Node *curr = head;

    while (curr) {
      if (curr->key == key) {
        break;
      }
      prev = curr;
      curr = curr->next.load(std::memory_order_seq_cst);
    }

    if (!curr) {
      return false;
    }

    Node *next_node = curr->next.load(std::memory_order_seq_cst);
    if (prev) {
      if (prev->next.compare_exchange_weak(curr, next_node,
                                           std::memory_order_seq_cst,
                                           std::memory_order_seq_cst)) {
        size_.fetch_sub(1, std::memory_order_relaxed);
        HazardPointerRegistry::GetInstance().Retire(curr, allocator_);
        return true;
      }
    } else {
      if (buckets_[bucket_idx].compare_exchange_weak(
              head, next_node, std::memory_order_seq_cst,
              std::memory_order_seq_cst)) {
        size_.fetch_sub(1, std::memory_order_relaxed);
        HazardPointerRegistry::GetInstance().Retire(curr, allocator_);
        return true;
      }
    }
  }
}

void LockFreeKVStore::Clear() {
  for (size_t i = 0; i < num_buckets_; ++i) {
    Node *curr = buckets_[i].exchange(nullptr, std::memory_order_acq_rel);
    while (curr) {
      Node *next = curr->next.load(std::memory_order_relaxed);
      allocator_.Deallocate(curr);
      curr = next;
    }
  }
  size_.store(0, std::memory_order_relaxed);
}

size_t LockFreeKVStore::Size() const {
  return size_.load(std::memory_order_relaxed);
}

} // namespace fastkv
