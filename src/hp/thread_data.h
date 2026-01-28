#pragma once

#include <atomic>
#include <cstddef>
#include "guard.h"
#include "retired.h"
#include "thread_hp_storage.h"
#include "../opt/cache_line.h"

namespace detail {
namespace hp {

struct thread_data {
    thread_hp_storage hazards;
    retired_array retired;
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned int> sync_;

    thread_data(guard* guards, size_t guard_count, retired_ptr* retired_arr,
                size_t retired_capacity)
        : hazards(guards, guard_count),
          retired(retired_arr, retired_capacity),
          sync_(0) {}

    thread_data(const thread_data&) = delete;
    thread_data& operator=(const thread_data&) = delete;

    void sync() { sync_.fetch_add(1, std::memory_order_acq_rel); }
    guard* get_guards() const { return hazards.begin(); }
    retired_ptr* get_retired() const { return retired.first(); }
};

class default_tls_manager {
public:
    static thread_data* get_tls() { return tls_; }
    static void set_tls(thread_data* td) { tls_ = td; }

private:
    static inline thread_local thread_data* tls_ = nullptr;
};

}  // namespace hp
}  // namespace detail
