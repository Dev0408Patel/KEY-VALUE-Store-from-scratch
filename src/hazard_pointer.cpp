#include "fastkv/hazard_pointer.h"
#include <algorithm>
#include <stdexcept>

namespace fastkv {

// Define thread-local variables
thread_local std::vector<Node*> HazardPointerRegistry::retired_list_;
thread_local HazardPointerRegistry::ThreadRecord* HazardPointerRegistry::my_record_{nullptr};

struct ThreadRecordCleanup {
    HazardPointerRegistry::ThreadRecord* rec{nullptr};
    ~ThreadRecordCleanup() {
        if (rec) {
            rec->hp[0].store(nullptr, std::memory_order_release);
            rec->hp[1].store(nullptr, std::memory_order_release);
            rec->active.store(false, std::memory_order_release);
        }
    }
};

static thread_local ThreadRecordCleanup cleanup_helper;

HazardPointerRegistry& HazardPointerRegistry::GetInstance() {
    static HazardPointerRegistry instance;
    return instance;
}

HazardPointerRegistry::HazardPointerRegistry() {
    for (size_t i = 0; i < MAX_THREADS; ++i) {
        records_[i].hp[0].store(nullptr, std::memory_order_relaxed);
        records_[i].hp[1].store(nullptr, std::memory_order_relaxed);
        records_[i].active.store(false, std::memory_order_relaxed);
    }
}

HazardPointerRegistry::~HazardPointerRegistry() {
    // Note: Slabs deallocate their raw memory, so we only need to clear indices.
}

HazardPointerRegistry::ThreadRecord* HazardPointerRegistry::GetThreadRecord() {
    if (my_record_) {
        return my_record_;
    }
    
    for (size_t i = 0; i < MAX_THREADS; ++i) {
        bool expected = false;
        if (records_[i].active.compare_exchange_strong(expected, true,
                                                       std::memory_order_seq_cst,
                                                       std::memory_order_relaxed)) {
            my_record_ = &records_[i];
            cleanup_helper.rec = my_record_;
            return my_record_;
        }
    }
    
    throw std::runtime_error("Exceeded maximum threads in Hazard Pointer Registry");
}

void HazardPointerRegistry::Protect(size_t slot, Node* node) {
    ThreadRecord* rec = GetThreadRecord();
    if (rec) {
        rec->hp[slot].store(node, std::memory_order_seq_cst);
    }
}

void HazardPointerRegistry::Unprotect(size_t slot) {
    ThreadRecord* rec = GetThreadRecord();
    if (rec) {
        rec->hp[slot].store(nullptr, std::memory_order_seq_cst);
    }
}

void HazardPointerRegistry::Retire(Node* node, SlabAllocator& allocator) {
    if (!node) return;
    
    retired_list_.push_back(node);
    if (retired_list_.size() >= RETIRED_THRESHOLD) {
        Scan(allocator);
    }
}

void HazardPointerRegistry::ForceReclaim(SlabAllocator& allocator) {
    Scan(allocator);
}

void HazardPointerRegistry::Scan(SlabAllocator& allocator) {
    std::vector<Node*> active_hps;
    active_hps.reserve(MAX_THREADS * 2);
    
    for (size_t i = 0; i < MAX_THREADS; ++i) {
        if (records_[i].active.load(std::memory_order_seq_cst)) {
            Node* hp0 = records_[i].hp[0].load(std::memory_order_seq_cst);
            Node* hp1 = records_[i].hp[1].load(std::memory_order_seq_cst);
            if (hp0) active_hps.push_back(hp0);
            if (hp1) active_hps.push_back(hp1);
        }
    }
    
    std::sort(active_hps.begin(), active_hps.end());
    
    std::vector<Node*> remaining;
    for (Node* node : retired_list_) {
        if (std::binary_search(active_hps.begin(), active_hps.end(), node)) {
            remaining.push_back(node);
        } else {
            allocator.Deallocate(node);
        }
    }
    
    retired_list_ = std::move(remaining);
}

} // namespace fastkv
