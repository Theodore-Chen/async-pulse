#pragma once

#include <atomic>
#include "thread_data.h"

namespace detail {
namespace hp {

class smr {
public:
    static constexpr size_t kDefaultHazardPtrCount = 8;
    static constexpr size_t kDefaultMaxThreadCount = 128;
    static constexpr size_t kDefaultMaxRetiredPtrCount = 100;

    static smr& instance() {
        if (instance_ == nullptr) {
            construct();
        }
        return *instance_;
    }

    static bool is_initialized() noexcept {
        return instance_ != nullptr;
    }

    static void construct(size_t hazard_ptr_count = kDefaultHazardPtrCount,
                          size_t max_thread_count = kDefaultMaxThreadCount,
                          size_t max_retired_ptr_count = kDefaultMaxRetiredPtrCount);

    static void destruct();

    size_t hazard_ptr_count() const noexcept { return hazard_ptr_count_; }
    size_t max_thread_count() const noexcept { return max_thread_count_; }
    size_t max_retired_ptr_count() const noexcept { return max_retired_ptr_count_; }

    void scan(thread_data* rec);
    void help_scan(thread_data* this_rec);
    thread_data* alloc_thread_data();
    void free_thread_data(thread_data* rec, bool call_help_scan);

private:
    smr(size_t hazard_ptr_count, size_t max_thread_count, size_t max_retired_ptr_count);
    ~smr();

    smr(const smr&) = delete;
    smr& operator=(const smr&) = delete;
    smr(smr&&) = delete;
    smr& operator=(smr&&) = delete;

    void classic_scan(thread_data* rec);
    void inplace_scan(thread_data* rec);
    bool is_protected(void* ptr) const;

    static smr* instance_;

    struct thread_record {
        thread_data* data;
        thread_record* next;
        std::atomic<bool> active;
        thread_record(thread_data* d) : data(d), next(nullptr), active(true) {}
    };

    std::atomic<thread_record*> thread_list_;
    size_t const hazard_ptr_count_;
    size_t const max_thread_count_;
    size_t const max_retired_ptr_count_;
};

}  // namespace hp
}  // namespace detail
