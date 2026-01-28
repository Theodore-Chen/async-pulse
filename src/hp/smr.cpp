#include "smr.h"
#include <algorithm>
#include <cstdlib>
#include <vector>

namespace detail {
namespace hp {

smr* smr::instance_ = nullptr;

void smr::construct(size_t hazard_ptr_count, size_t max_thread_count, size_t max_retired_ptr_count) {
    if (instance_) return;
    instance_ = new smr(hazard_ptr_count, max_thread_count, max_retired_ptr_count);
}

void smr::destruct() {
    delete instance_;
    instance_ = nullptr;
}

smr::smr(size_t hazard_ptr_count, size_t max_thread_count, size_t max_retired_ptr_count)
    : thread_list_(nullptr),
      hazard_ptr_count_(hazard_ptr_count),
      max_thread_count_(max_thread_count),
      max_retired_ptr_count_(max_retired_ptr_count) {}

smr::~smr() {
    thread_record* rec = thread_list_.load(std::memory_order_acquire);
    while (rec) {
        thread_record* next = rec->next;

        guard* guards = rec->data->get_guards();
        retired_ptr* retired = rec->data->get_retired();

        for (size_t i = 0; i < hazard_ptr_count_; ++i) {
            guards[i].~guard();
        }
        ::operator delete[](guards);
        ::operator delete[](retired);

        rec->data->~thread_data();
        ::operator delete(rec->data);

        delete rec;
        rec = next;
    }
}

thread_data* smr::alloc_thread_data() {
    guard* guards = reinterpret_cast<guard*>(
        ::operator new[](sizeof(guard) * hazard_ptr_count_));
    new (guards) guard[hazard_ptr_count_];

    retired_ptr* retired = reinterpret_cast<retired_ptr*>(
        ::operator new[](sizeof(retired_ptr) * max_retired_ptr_count_));

    void* td_mem = ::operator new(sizeof(thread_data));
    thread_data* td = reinterpret_cast<thread_data*>(td_mem);
    new (td) thread_data(guards, hazard_ptr_count_, retired, max_retired_ptr_count_);

    thread_record* new_rec = new thread_record(td);
    thread_record* old_head = thread_list_.load(std::memory_order_relaxed);
    do {
        new_rec->next = old_head;
    } while (!thread_list_.compare_exchange_weak(old_head, new_rec, std::memory_order_release,
                                                 std::memory_order_relaxed));

    return td;
}

void smr::free_thread_data(thread_data* rec, bool call_help_scan) {
    if (call_help_scan) {
        help_scan(rec);
    }

    guard* guards = rec->get_guards();
    retired_ptr* retired = rec->get_retired();

    for (size_t i = 0; i < hazard_ptr_count_; ++i) {
        guards[i].~guard();
    }
    ::operator delete[](guards);
    ::operator delete[](retired);

    rec->~thread_data();
    ::operator delete(rec);

    thread_record* prev = nullptr;
    thread_record* curr = thread_list_.load(std::memory_order_acquire);
    while (curr) {
        if (curr->data == rec) {
            if (prev) {
                prev->next = curr->next;
            } else {
                thread_list_.store(curr->next, std::memory_order_release);
            }
            curr->active.store(false, std::memory_order_release);
            delete curr;
            break;
        }
        prev = curr;
        curr = curr->next;
    }
}

bool smr::is_protected(void* ptr) const {
    thread_record* rec = thread_list_.load(std::memory_order_acquire);
    while (rec) {
        if (rec->active.load(std::memory_order_acquire)) {
            thread_data* td = rec->data;
            for (guard* g = td->hazards.begin(), *last = td->hazards.end(); g != last; ++g) {
                if (g->get() == ptr) {
                    return true;
                }
            }
        }
        rec = rec->next;
    }
    return false;
}

void smr::classic_scan(thread_data* rec) {
    rec->sync();

    std::vector<void*> hp_list;
    thread_record* tr = thread_list_.load(std::memory_order_acquire);
    while (tr) {
        if (tr->active.load(std::memory_order_acquire)) {
            thread_data* td = tr->data;
            for (guard* g = td->hazards.begin(), *last = td->hazards.end(); g != last; ++g) {
                void* p = g->get();
                if (p != nullptr) {
                    hp_list.push_back(p);
                }
            }
        }
        tr = tr->next;
    }

    std::sort(hp_list.begin(), hp_list.end());
    auto hp_end = std::unique(hp_list.begin(), hp_list.end());

    retired_ptr* src = rec->retired.first();
    retired_ptr* dst = src;
    retired_ptr* end = rec->retired.last();

    while (src != end) {
        if (std::binary_search(hp_list.begin(), hp_end, src->ptr)) {
            *dst = *src;
            ++dst;
        } else {
            src->deleter(src->ptr);
        }
        ++src;
    }

    rec->retired.reset(dst - rec->retired.first());
}

void smr::inplace_scan(thread_data* rec) {
    rec->sync();

    retired_ptr* first = rec->retired.first();
    retired_ptr* last = rec->retired.last();
    retired_ptr* new_last = first;

    for (retired_ptr* r = first; r != last; ++r) {
        bool is_protected = false;

        thread_record* tr = thread_list_.load(std::memory_order_acquire);
        while (tr && !is_protected) {
            if (tr->active.load(std::memory_order_acquire)) {
                thread_data* td = tr->data;
                for (guard* g = td->hazards.begin(), *end = td->hazards.end(); g != end; ++g) {
                    if (g->get() == r->ptr) {
                        is_protected = true;
                        break;
                    }
                }
            }
            tr = tr->next;
        }

        if (is_protected) {
            if (new_last != r) {
                *new_last = *r;
            }
            ++new_last;
        } else {
            r->deleter(r->ptr);
        }
    }

    rec->retired.reset(new_last - first);
}

void smr::scan(thread_data* rec) {
    inplace_scan(rec);
}

void smr::help_scan(thread_data* this_rec) {
    thread_record* tr = thread_list_.load(std::memory_order_acquire);
    while (tr) {
        if (!tr->active.load(std::memory_order_acquire) && tr->data != this_rec) {
            thread_data* rec = tr->data;

            retired_ptr* first = rec->retired.first();
            retired_ptr* last = rec->retired.last();

            for (retired_ptr* r = first; r != last; ++r) {
                if (r->ptr != nullptr && r->deleter != nullptr) {
                    r->deleter(r->ptr);
                    r->ptr = nullptr;
                    r->deleter = nullptr;
                }
            }

            rec->retired.reset(0);
        }
        tr = tr->next;
    }
}

}  // namespace hp
}  // namespace detail
