// 队列性能基准测试

#include <benchmark/benchmark.h>

#include "queue_factory.h"
#include "queue_helpers.h"
#include "test_types.h"
#include "thread_helpers.h"
#include "thread_sync.h"
#include "queue/lock_bounded_queue.h"
#include "queue/lock_free_bounded_queue.h"
#include "queue/lock_queue.h"
#include "queue/ms_queue.h"

using namespace benchmark;

// 单线程往返测试
template <typename QueueType>
static void bm_single_thread_round_trip_int(benchmark::State& state) {
    auto q = queue_factory<QueueType, 1024>::create();
    int value = 42;
    int result;

    for (auto _ : state) {
        q->enqueue(value);
        q->dequeue(result);
    }

    state.SetItemsProcessed(state.iterations() * 2);
    state.SetBytesProcessed(state.iterations() * 2 * sizeof(int));
}

template <typename QueueType>
static void bm_round_trip_small_object(benchmark::State& state) {
    auto q = queue_factory<QueueType, 1024>::create();
    small_object value{42};
    small_object result;

    for (auto _ : state) {
        q->enqueue(value);
        q->dequeue(result);
    }

    state.SetItemsProcessed(state.iterations() * 2);
    state.SetBytesProcessed(state.iterations() * 2 * sizeof(small_object));
}

template <typename QueueType>
static void bm_round_trip_medium_object(benchmark::State& state) {
    auto q = queue_factory<QueueType, 1024>::create();
    medium_object value{42};
    medium_object result;

    for (auto _ : state) {
        q->enqueue(value);
        q->dequeue(result);
    }

    state.SetItemsProcessed(state.iterations() * 2);
    state.SetBytesProcessed(state.iterations() * 2 * sizeof(medium_object));
}

template <typename QueueType>
static void bm_round_trip_large_object(benchmark::State& state) {
    auto q = queue_factory<QueueType, 1024>::create();
    large_object value{42};
    large_object result;

    for (auto _ : state) {
        q->enqueue(value);
        q->dequeue(result);
    }

    state.SetItemsProcessed(state.iterations() * 2);
    state.SetBytesProcessed(state.iterations() * 2 * sizeof(large_object));
}

// 容量测试（仅适用于有界队列）
template <typename QueueType>
static void bm_capacity(benchmark::State& state) {
    QueueType q(state.range(0));
    int value = 42;
    int result;

    for (auto _ : state) {
        q.enqueue(value);
        q.dequeue(result);
    }
}

// SPSC: 单生产者单消费者
template <typename QueueType>
static void bm_spsc(benchmark::State& state) {
    constexpr size_t ITEM_NUM = 1024 * 16;
    const size_t PRODUCER_NUM = 1;
    const size_t CONSUMER_NUM = 1;

    for (auto _ : state) {
        auto q = queue_factory<QueueType, 1024>::create();
        start_sync sync;

        state.PauseTiming();
        sync.set_expected_count(PRODUCER_NUM + CONSUMER_NUM);
        std::vector<std::future<void>> consumers = create_consumers(*q, CONSUMER_NUM, sync);
        std::vector<std::future<void>> producers = create_producers(*q, PRODUCER_NUM, ITEM_NUM, sync);
        sync.wait_until_all_ready();
        state.ResumeTiming();

        sync.notify_all();

        producers.clear();
        q->close();
        consumers.clear();
    }

    state.SetItemsProcessed(state.iterations() * ITEM_NUM);
}

// MPSC: 多生产者单消费者
template <typename QueueType>
static void bm_mpsc(benchmark::State& state) {
    const size_t PRODUCER_NUM = state.range(0);
    constexpr size_t ITEM_NUM = 1024 * 16;
    const size_t CONSUMER_NUM = 1;

    for (auto _ : state) {
        auto q = queue_factory<QueueType, 1024>::create();
        start_sync sync;

        state.PauseTiming();
        sync.set_expected_count(PRODUCER_NUM + CONSUMER_NUM);
        std::vector<std::future<void>> consumers = create_consumers(*q, CONSUMER_NUM, sync);
        std::vector<std::future<void>> producers = create_producers(*q, PRODUCER_NUM, ITEM_NUM, sync);
        sync.wait_until_all_ready();
        state.ResumeTiming();

        sync.notify_all();

        producers.clear();
        q->close();
        consumers.clear();
    }

    state.SetItemsProcessed(state.iterations() * ITEM_NUM * PRODUCER_NUM);
}

// SPMC: 单生产者多消费者
template <typename QueueType>
static void bm_spmc(benchmark::State& state) {
    const size_t CONSUMER_NUM = state.range(0);
    constexpr size_t ITEM_NUM = 1024 * 16;
    const size_t PRODUCER_NUM = 1;

    for (auto _ : state) {
        auto q = queue_factory<QueueType, 1024>::create();
        start_sync sync;

        state.PauseTiming();
        sync.set_expected_count(PRODUCER_NUM + CONSUMER_NUM);
        std::vector<std::future<void>> consumers = create_consumers(*q, CONSUMER_NUM, sync);
        std::vector<std::future<void>> producers = create_producers(*q, PRODUCER_NUM, ITEM_NUM, sync);
        sync.wait_until_all_ready();
        state.ResumeTiming();

        sync.notify_all();

        producers.clear();
        q->close();
        consumers.clear();
    }

    state.SetItemsProcessed(state.iterations() * ITEM_NUM);
}

// MPMC: 多生产者多消费者
template <typename QueueType>
static void bm_mpmc(benchmark::State& state) {
    const size_t PRODUCER_NUM = state.range(0);
    const size_t CONSUMER_NUM = state.range(0);
    constexpr size_t ITEM_NUM = 1024 * 16;

    if (PRODUCER_NUM == 0 || CONSUMER_NUM == 0) {
        state.SkipWithError("Need at least 1 producer and 1 consumer");
        return;
    }

    for (auto _ : state) {
        auto q = queue_factory<QueueType, 1024>::create();
        start_sync sync;

        state.PauseTiming();
        sync.set_expected_count(PRODUCER_NUM + CONSUMER_NUM);
        std::vector<std::future<void>> consumers = create_consumers(*q, CONSUMER_NUM, sync);
        std::vector<std::future<void>> producers = create_producers(*q, PRODUCER_NUM, ITEM_NUM, sync);
        sync.wait_until_all_ready();
        state.ResumeTiming();

        sync.notify_all();

        producers.clear();
        q->close();
        consumers.clear();
    }

    state.SetItemsProcessed(state.iterations() * ITEM_NUM * PRODUCER_NUM);
}

// 压力测试：队列接近满时的性能
template <typename QueueType>
static void bm_near_full_90_percent(benchmark::State& state) {
    auto q = queue_factory<QueueType, 1024>::create();
    fill_queue_to_percentage(*q, 0.9);

    for (auto _ : state) {
        int value;
        q->dequeue(value);
        q->enqueue(42);
    }
}

template <typename QueueType>
static void bm_near_full_99_percent(benchmark::State& state) {
    auto q = queue_factory<QueueType, 1024>::create();
    fill_queue_to_percentage(*q, 0.99);

    for (auto _ : state) {
        int value;
        q->dequeue(value);
        q->enqueue(42);
    }
}

template <typename QueueType>
static void bm_empty_queue_try_dequeue(benchmark::State& state) {
    auto q = queue_factory<QueueType, 1024>::create();
    int value;

    for (auto _ : state) {
        q->try_dequeue_with([&](int& v) {});
    }
}

// 注册基准测试

// 单线程
BENCHMARK(bm_single_thread_round_trip_int<lock_free_bounded_queue<int>>);
BENCHMARK(bm_single_thread_round_trip_int<lock_bounded_queue<int>>);
BENCHMARK(bm_single_thread_round_trip_int<lock_queue<int>>);
BENCHMARK(bm_single_thread_round_trip_int<ms_queue<int>>);

BENCHMARK(bm_round_trip_small_object<lock_free_bounded_queue<small_object>>);
BENCHMARK(bm_round_trip_small_object<lock_bounded_queue<small_object>>);
BENCHMARK(bm_round_trip_small_object<lock_queue<small_object>>);
BENCHMARK(bm_round_trip_small_object<ms_queue<small_object>>);
BENCHMARK(bm_round_trip_medium_object<lock_free_bounded_queue<medium_object>>);
BENCHMARK(bm_round_trip_medium_object<lock_bounded_queue<medium_object>>);
BENCHMARK(bm_round_trip_medium_object<lock_queue<medium_object>>);
BENCHMARK(bm_round_trip_medium_object<ms_queue<medium_object>>);
BENCHMARK(bm_round_trip_large_object<lock_free_bounded_queue<large_object>>);
BENCHMARK(bm_round_trip_large_object<lock_bounded_queue<large_object>>);
BENCHMARK(bm_round_trip_large_object<lock_queue<large_object>>);
BENCHMARK(bm_round_trip_large_object<ms_queue<large_object>>);

// 容量测试
BENCHMARK_TEMPLATE(bm_capacity, lock_free_bounded_queue<int>)->Range(64, 4096);
BENCHMARK_TEMPLATE(bm_capacity, lock_bounded_queue<int>)->Range(64, 4096);

// SPSC
BENCHMARK(bm_spsc<lock_free_bounded_queue<int>>);
BENCHMARK(bm_spsc<lock_bounded_queue<int>>);
BENCHMARK(bm_spsc<lock_queue<int>>);
BENCHMARK(bm_spsc<ms_queue<int>>);

// MPSC: 2, 4, 16 生产者
BENCHMARK_TEMPLATE(bm_mpsc, lock_free_bounded_queue<int>)->Args({2})->Args({4})->Args({16});
BENCHMARK_TEMPLATE(bm_mpsc, lock_bounded_queue<int>)->Args({2})->Args({4})->Args({16});
BENCHMARK_TEMPLATE(bm_mpsc, lock_queue<int>)->Args({2})->Args({4})->Args({16});
BENCHMARK_TEMPLATE(bm_mpsc, ms_queue<int>)->Args({2})->Args({4})->Args({16});

// SPMC: 2, 4, 16 消费者
BENCHMARK_TEMPLATE(bm_spmc, lock_free_bounded_queue<int>)->Args({2})->Args({4})->Args({16});
BENCHMARK_TEMPLATE(bm_spmc, lock_bounded_queue<int>)->Args({2})->Args({4})->Args({16});
BENCHMARK_TEMPLATE(bm_spmc, lock_queue<int>)->Args({2})->Args({4})->Args({16});
BENCHMARK_TEMPLATE(bm_spmc, ms_queue<int>)->Args({2})->Args({4})->Args({16});

// MPMC: 2, 4, 16 生产者+消费者
BENCHMARK_TEMPLATE(bm_mpmc, lock_free_bounded_queue<int>)->Args({2})->Args({4})->Args({16});
BENCHMARK_TEMPLATE(bm_mpmc, lock_bounded_queue<int>)->Args({2})->Args({4})->Args({16});
BENCHMARK_TEMPLATE(bm_mpmc, lock_queue<int>)->Args({2})->Args({4})->Args({16});
BENCHMARK_TEMPLATE(bm_mpmc, ms_queue<int>)->Args({2})->Args({4})->Args({16});

// 压力测试
BENCHMARK(bm_near_full_90_percent<lock_free_bounded_queue<int>>);
BENCHMARK(bm_near_full_90_percent<lock_bounded_queue<int>>);
BENCHMARK(bm_near_full_99_percent<lock_free_bounded_queue<int>>);
BENCHMARK(bm_near_full_99_percent<lock_bounded_queue<int>>);
BENCHMARK(bm_empty_queue_try_dequeue<lock_free_bounded_queue<int>>);
BENCHMARK(bm_empty_queue_try_dequeue<lock_bounded_queue<int>>);

BENCHMARK_MAIN();
