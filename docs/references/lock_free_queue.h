#include <atomic>
#include <cassert>
#include <cstddef>
#include <memory>
#include <new>
template <typename T, std::size_t min_capacity, typename Alloc = std::allocator<T>>
class FIFO : private Alloc {
   public:
    using value_type = T;
    using allocator_traits = std::allocator_traits<Alloc>;
    using size_type = typename allocator_traits::size_type;
    explicit FIFO(Alloc const& alloc = Alloc{}) : Alloc{alloc}, ring_{allocator_traits::allocate(*this, mask_ + 1)} {}
    ~FIFO() {
        while (!empty()) {
            element(popCursor_)->~T();
            ++popCursor_;
        }
        allocator_traits::deallocate(*this, ring_, mask_ + 1);
    }
    [[nodiscard]] auto size() const noexcept {
        auto pushCursor = pushCursor_.load(std::memory_order_relaxed);
        auto popCursor = popCursor_.load(std::memory_order_relaxed);
        assert(popCursor <= pushCursor);
        return pushCursor - popCursor;
    }
    [[nodiscard]] auto empty() const noexcept { return size() == 0; }
    [[nodiscard]] auto push(T const& value) {
        auto pushCursor = pushCursor_.load(std::memory_order_relaxed);
        if (full(pushCursor, popCursorCached_)) {
            popCursorCached_ = popCursor_.load(std::memory_order_acquire);
            if (full(pushCursor, popCursorCached_)) {
                return false;
            }
        }
        new (element(pushCursor)) T(value);
        pushCursor_.store(pushCursor + 1, std::memory_order_release);
        return true;
    }
    [[nodiscard]] auto pop(T& value) {
        auto popCursor = popCursor_.load(std::memory_order_relaxed);
        if (empty(pushCursorCached_, popCursor)) {
            pushCursorCached_ = pushCursor_.load(std::memory_order_acquire);
            if (empty(pushCursorCached_, popCursor)) {
                return false;
            }
        }
        value = *element(popCursor);
        element(popCursor)->~T();
        popCursor_.store(popCursor + 1, std::memory_order_release);
        return true;
    }

   private:
    auto full(size_type pushCursor, size_type popCursor) const noexcept {
        return (pushCursor - popCursor) == mask_ + 1;
    }
    static auto empty(size_type pushCursor, size_type popCursor) noexcept { return pushCursor == popCursor; }
    auto element(size_type cursor) noexcept { return &ring_[cursor & mask_]; }

   private:
    using CursorType = std::atomic<size_type>;
    static_assert(CursorType::is_always_lock_free, "CursorType::is_always_lock_free");
    static constexpr auto calc_mask() {
        static_assert(min_capacity > 0, "min_capacity > 0");
        size_type index = 0, mask = 0;
        for (size_type i = 0; i < sizeof(size_type) * 8; ++i) {
            if ((min_capacity >> i) & 1) {
                index = i;
            }
        }
        for (size_type i = 0; i <= index; ++i) {
            mask |= size_type(1) << i;
        }
        return mask;
    }
    static constexpr size_type mask_{calc_mask()};
    T* ring_;
    alignas(std::hardware_destructive_interference_size) CursorType pushCursor_;
    alignas(std::hardware_destructive_interference_size) size_type popCursorCached_{};
    alignas(std::hardware_destructive_interference_size) CursorType popCursor_;
    alignas(std::hardware_destructive_interference_size) size_type pushCursorCached_{};
    char padding_[std::hardware_destructive_interference_size - sizeof(size_type)];
};
