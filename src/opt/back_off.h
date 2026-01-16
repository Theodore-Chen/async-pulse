#pragma once

#include <thread>

struct back_off_default_traits {
    static constexpr size_t lower_bound = 16;
    static constexpr size_t upper_bound = 16 * 1024;
};

template <typename Traits = back_off_default_traits>
class back_off {
   public:
    void operator()() noexcept {
        if (cur_spin_ <= Traits::upper_bound) {
            for (size_t n = 0; n < cur_spin_; n++) {
                spin();
            }
            cur_spin_ *= 2;
        } else {
            yield();
        }
    }

   private:
    void spin() noexcept {
#if defined(__x86_64__) || defined(__i386__)
        asm volatile("pause;");
#elif defined(__aarch64__) || defined(__arm__)
        asm volatile("yield");
#endif
    }

    void yield() noexcept {
        std::this_thread::yield();
    }

   private:
    size_t cur_spin_ = Traits::lower_bound;
};
