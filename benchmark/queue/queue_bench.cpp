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

using detail::ms_queue;

constexpr size_t BULK_ITEM_COUNT = 1024 * 16;

template <typename QueueType>
void bm_single_thread_round_trip_int(benchmark::State& state) {
    auto q = queue_factory<QueueType, QUEUE_CAPACITY>::create();
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
void bm_round_trip_small_object(benchmark::State& state) {
    auto q = queue_factory<QueueType, QUEUE_CAPACITY>::create();
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
void bm_round_trip_medium_object(benchmark::State& state) {
    auto q = queue_factory<QueueType, QUEUE_CAPACITY>::create();
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
void bm_round_trip_large_object(benchmark::State& state) {
    auto q = queue_factory<QueueType, QUEUE_CAPACITY>::create();
    large_object value{42};
    large_object result;

    for (auto _ : state) {
        q->enqueue(value);
        q->dequeue(result);
    }

    state.SetItemsProcessed(state.iterations() * 2);
    state.SetBytesProcessed(state.iterations() * 2 * sizeof(large_object));
}

template <typename QueueType>
void bm_capacity(benchmark::State& state) {
    QueueType q(state.range(0));
    int value = 42;
    int result;

    for (auto _ : state) {
        q.enqueue(value);
        q.dequeue(result);
    }
}

template <typename Queue>
void run_producer_consumer_benchmark(benchmark::State& state,
                                      size_t producer_num,
                                      size_t consumer_num,
                                      size_t items_per_producer) {
    for (auto _ : state) {
        auto q = queue_factory<Queue, QUEUE_CAPACITY>::create();
        start_sync sync;

        state.PauseTiming();
        sync.set_expected_count(producer_num + consumer_num);
        auto consumers = create_consumers(*q, consumer_num, sync);
        auto producers = create_producers(*q, producer_num, items_per_producer, sync);
        sync.wait_until_all_ready();
        state.ResumeTiming();

        sync.notify_all();

        producers.clear();
        q->close();
        consumers.clear();
    }

    state.SetItemsProcessed(state.iterations() * items_per_producer * producer_num);
}

template <typename QueueType>
void bm_spsc(benchmark::State& state) {
    run_producer_consumer_benchmark<QueueType>(state, 1, 1, BULK_ITEM_COUNT);
}

template <typename QueueType>
void bm_mpsc(benchmark::State& state) {
    size_t producer_num = state.range(0);
    run_producer_consumer_benchmark<QueueType>(state, producer_num, 1, BULK_ITEM_COUNT);
}

template <typename QueueType>
void bm_spmc(benchmark::State& state) {
    size_t consumer_num = state.range(0);
    run_producer_consumer_benchmark<QueueType>(state, 1, consumer_num, BULK_ITEM_COUNT);
}

template <typename QueueType>
void bm_mpmc(benchmark::State& state) {
    size_t thread_num = state.range(0);
    if (thread_num == 0) {
        state.SkipWithError("Need at least 1 producer and 1 consumer");
        return;
    }
    run_producer_consumer_benchmark<QueueType>(state, thread_num, thread_num, BULK_ITEM_COUNT);
}

template <typename QueueType>
void bm_near_full_90_percent(benchmark::State& state) {
    auto q = queue_factory<QueueType, QUEUE_CAPACITY>::create();
    fill_queue_to_percentage(*q, 0.9);

    for (auto _ : state) {
        int value{};
        q->dequeue(value);
        q->enqueue(42);
    }
    
    state.SetItemsProcessed(state.iterations() * 2);
    state.SetBytesProcessed(state.iterations() * 2 * sizeof(int));
}

template <typename QueueType>
void bm_near_full_99_percent(benchmark::State& state) {
    auto q = queue_factory<QueueType, QUEUE_CAPACITY>::create();
    fill_queue_to_percentage(*q, 0.99);

    for (auto _ : state) {
        int value{};
        q->dequeue(value);
        q->enqueue(42);
    }
    
    state.SetItemsProcessed(state.iterations() * 2);
    state.SetBytesProcessed(state.iterations() * 2 * sizeof(int));
}

template <typename QueueType>
void bm_empty_queue_try_dequeue(benchmark::State& state) {
    auto q = queue_factory<QueueType, QUEUE_CAPACITY>::create();
    int value{};

    for (auto _ : state) {
        q->try_dequeue_with([&](int& v) {});
    }
    
    state.SetItemsProcessed(state.iterations());
}

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

BENCHMARK_TEMPLATE(bm_capacity, lock_free_bounded_queue<int>)->Range(64, 4096);
BENCHMARK_TEMPLATE(bm_capacity, lock_bounded_queue<int>)->Range(64, 4096);

BENCHMARK(bm_spsc<lock_free_bounded_queue<int>>);
BENCHMARK(bm_spsc<lock_bounded_queue<int>>);
BENCHMARK(bm_spsc<lock_queue<int>>);
BENCHMARK(bm_spsc<ms_queue<int>>);

BENCHMARK_TEMPLATE(bm_mpsc, lock_free_bounded_queue<int>)->Args({2})->Args({4})->Args({16});
BENCHMARK_TEMPLATE(bm_mpsc, lock_bounded_queue<int>)->Args({2})->Args({4})->Args({16});
BENCHMARK_TEMPLATE(bm_mpsc, lock_queue<int>)->Args({2})->Args({4})->Args({16});
BENCHMARK_TEMPLATE(bm_mpsc, ms_queue<int>)->Args({2})->Args({4})->Args({16});

BENCHMARK_TEMPLATE(bm_spmc, lock_free_bounded_queue<int>)->Args({2})->Args({4})->Args({16});
BENCHMARK_TEMPLATE(bm_spmc, lock_bounded_queue<int>)->Args({2})->Args({4})->Args({16});
BENCHMARK_TEMPLATE(bm_spmc, lock_queue<int>)->Args({2})->Args({4})->Args({16});
BENCHMARK_TEMPLATE(bm_spmc, ms_queue<int>)->Args({2})->Args({4})->Args({16});

BENCHMARK_TEMPLATE(bm_mpmc, lock_free_bounded_queue<int>)->Args({2})->Args({4})->Args({16});
BENCHMARK_TEMPLATE(bm_mpmc, lock_bounded_queue<int>)->Args({2})->Args({4})->Args({16});
BENCHMARK_TEMPLATE(bm_mpmc, lock_queue<int>)->Args({2})->Args({4})->Args({16});
BENCHMARK_TEMPLATE(bm_mpmc, ms_queue<int>)->Args({2})->Args({4})->Args({16});

BENCHMARK(bm_near_full_90_percent<lock_free_bounded_queue<int>>);
BENCHMARK(bm_near_full_90_percent<lock_bounded_queue<int>>);
BENCHMARK(bm_near_full_90_percent<ms_queue<int>>);
BENCHMARK(bm_near_full_90_percent<lock_queue<int>>);

BENCHMARK(bm_near_full_99_percent<lock_free_bounded_queue<int>>);
BENCHMARK(bm_near_full_99_percent<lock_bounded_queue<int>>);
BENCHMARK(bm_near_full_99_percent<ms_queue<int>>);
BENCHMARK(bm_near_full_99_percent<lock_queue<int>>);

BENCHMARK(bm_empty_queue_try_dequeue<lock_free_bounded_queue<int>>);
BENCHMARK(bm_empty_queue_try_dequeue<lock_bounded_queue<int>>);
BENCHMARK(bm_empty_queue_try_dequeue<ms_queue<int>>);
BENCHMARK(bm_empty_queue_try_dequeue<lock_queue<int>>);

BENCHMARK_MAIN();
