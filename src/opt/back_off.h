#pragma once

#include <thread>

class back_off {
   public:
    void operator()() noexcept {
        if (cur_spin_ <= upper_bound_) {
            for (size_t n = 0; n < cur_spin_; n++) {
                spin();
            }
            cur_spin_ *= 2;
        } else {
            yield();
        }
    }

   private:
    void spin() {
#if defined(__x86_64__) || defined(__i386__)
        asm volatile("pause;");
#elif defined(__aarch64__) || defined(__arm__)
        asm volatile("yield");
#endif
    }

    void yield() {
        std::this_thread::yield();
    }

   private:
    size_t lower_bound_ = 16;
    size_t upper_bound_ = 16 * 1024;
    size_t cur_spin_ = lower_bound_;
};
