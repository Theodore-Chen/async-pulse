#pragma once

#include <atomic>
#include <cstddef>
#include <cassert>
#include "guard.h"
#include "guard_array.h"
#include "retired.h"
#include "smr.h"
#include "thread_data.h"

namespace detail {
namespace hp {

template <typename TLSManager = default_tls_manager>
class generic_hp {
public:
    using tls_manager = TLSManager;

    class guard {
    public:
        guard() {
            attach_thread();
            thread_data* td = tls_manager::get_tls();
            guard_ = td->hazards.alloc();
        }

        explicit guard(std::nullptr_t) noexcept : guard_(nullptr) {}

        guard(guard&& other) noexcept : guard_(other.guard_) {
            other.guard_ = nullptr;
        }

        guard& operator=(guard&& other) noexcept {
            std::swap(guard_, other.guard_);
            return *this;
        }

        guard(const guard&) = delete;
        guard& operator=(const guard&) = delete;

        ~guard() {
            if (guard_) {
                thread_data* td = tls_manager::get_tls();
                if (td) {
                    td->hazards.free(guard_);
                }
            }
        }

        bool is_linked() const { return guard_ != nullptr; }

        template <typename T>
        T* protect(std::atomic<T*> const& to_guard) {
            return protect(to_guard, [](T* p) { return p; });
        }

        template <typename T, typename Func>
        T* protect(std::atomic<T*> const& to_guard, Func func) {
            assert(guard_ != nullptr);

            T* p_cur = to_guard.load(std::memory_order_relaxed);
            T* p_ret;
            do {
                p_ret = p_cur;
                guard_->set(func(p_cur));
                p_cur = to_guard.load(std::memory_order_acquire);
            } while (p_ret != p_cur);
            return p_cur;
        }

        template <typename T>
        T* assign(T* p) {
            assert(guard_ != nullptr);
            guard_->set(p);
            tls_manager::get_tls()->sync();
            return p;
        }

        std::nullptr_t assign(std::nullptr_t) {
            assert(guard_ != nullptr);
            guard_->clear();
            return nullptr;
        }

        void clear() { assign(nullptr); }

        template <typename T>
        T* get() const {
            assert(guard_ != nullptr);
            return guard_->get_as<T>();
        }

        void* get_native() const {
            assert(guard_ != nullptr);
            return guard_->get();
        }

        ::detail::hp::guard* release() noexcept {
            ::detail::hp::guard* g = guard_;
            guard_ = nullptr;
            return g;
        }

        ::detail::hp::guard*& guard_ref() { return guard_; }

    private:
        ::detail::hp::guard* guard_;
    };

    template <size_t Count>
    class scoped_guards {
    public:
        static constexpr size_t c_nCapacity = Count;

        scoped_guards() {
            attach_thread();
            thread_data* td = tls_manager::get_tls();
            td->hazards.alloc(guards_);
        }

        ~scoped_guards() {
            thread_data* td = tls_manager::get_tls();
            if (td) {
                td->hazards.free(guards_);
            }
        }

        scoped_guards(const scoped_guards&) = delete;
        scoped_guards& operator=(const scoped_guards&) = delete;
        scoped_guards(scoped_guards&&) = delete;
        scoped_guards& operator=(scoped_guards&&) = delete;

        static constexpr size_t capacity() { return c_nCapacity; }

        template <typename T>
        T* protect(size_t idx, std::atomic<T*> const& to_guard) {
            return protect(idx, to_guard, [](T* p) { return p; });
        }

        template <typename T, typename Func>
        T* protect(size_t idx, std::atomic<T*> const& to_guard, Func func) {
            assert(idx < capacity());

            T* p_ret;
            do {
                assign(idx, func(p_ret = to_guard.load(std::memory_order_relaxed)));
            } while (p_ret != to_guard.load(std::memory_order_acquire));

            return p_ret;
        }

        template <typename T>
        T* assign(size_t idx, T* p) {
            assert(idx < capacity());
            guards_.set(idx, p);
            tls_manager::get_tls()->sync();
            return p;
        }

        void clear(size_t idx) { guards_.clear(idx); }

        template <typename U>
        U* get(size_t idx) const {
            assert(idx < capacity());
            return guards_[idx]->template get_as<U>();
        }

        void* get_native(size_t idx) const {
            assert(idx < capacity());
            return guards_[idx]->get();
        }

    private:
        ::detail::hp::guard_array<Count> guards_;
    };

    template <size_t Count>
    using guard_array = scoped_guards<Count>;

    static void construct(size_t hazard_ptr_count = smr::kDefaultHazardPtrCount,
                          size_t max_thread_count = smr::kDefaultMaxThreadCount,
                          size_t max_retired_ptr_count = smr::kDefaultMaxRetiredPtrCount) {
        smr::construct(hazard_ptr_count, max_thread_count, max_retired_ptr_count);
    }

    static void destruct() { smr::destruct(); }

    static void attach_thread() {
        smr::instance();
        if (!tls_manager::get_tls()) {
            tls_manager::set_tls(smr::instance().alloc_thread_data());
        }
    }

    static void detach_thread() {
        thread_data* rec = tls_manager::get_tls();
        if (rec) {
            tls_manager::set_tls(nullptr);
            smr::instance().free_thread_data(rec, true);
        }
    }

    template <typename T>
    static void retire(T* p, void (*func)(void*)) {
        attach_thread();
        thread_data* rec = tls_manager::get_tls();
        assert(rec != nullptr);

        if (!rec->retired.push(retired_ptr(p, func))) {
            smr::instance().scan(rec);
        }
    }

    template <typename Disposer, typename T>
    static void retire(T* p) {
        retire(p, +[](void* ptr) { Disposer()(static_cast<T*>(ptr)); });
    }

    static void scan() {
        thread_data* rec = tls_manager::get_tls();
        assert(rec != nullptr);
        smr::instance().scan(rec);
    }

    static size_t max_hazard_count() {
        return smr::instance().hazard_ptr_count();
    }

    static size_t max_thread_count() {
        return smr::instance().max_thread_count();
    }

    static size_t retired_array_capacity() {
        return smr::instance().max_retired_ptr_count();
    }
};

}  // namespace hp
}  // namespace detail
