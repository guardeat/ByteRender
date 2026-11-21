#include "common/retire_queue.hpp"

#include <doctest/doctest.h>

#include <atomic>
#include <thread>
#include <vector>

using byte::render::rhi::detail::is_reached;
using byte::render::rhi::detail::RetireQueue;

TEST_CASE("is_reached requires every queue to reach its value") {
    CHECK(is_reached<2>({0, 0}, {0, 0}));
    CHECK(is_reached<2>({3, 5}, {3, 5}));
    CHECK(is_reached<2>({3, 5}, {4, 9}));
    CHECK_FALSE(is_reached<2>({3, 5}, {2, 9}));
    CHECK_FALSE(is_reached<2>({3, 5}, {9, 4}));
}

TEST_CASE("RetireQueue destroys objects only after the GPU completed their values, in order") {
    RetireQueue<1> queue;
    std::vector<int> destroyed;

    queue.push({1}, [&] { destroyed.push_back(1); });
    queue.push({1}, [&] { destroyed.push_back(2); });
    queue.push({3}, [&] { destroyed.push_back(3); });
    CHECK(queue.size() == 3);

    CHECK(queue.collect({0}) == 0);
    CHECK(destroyed.empty());

    CHECK(queue.collect({2}) == 2);
    CHECK(destroyed == std::vector<int>{1, 2});

    CHECK(queue.collect({2}) == 0);
    CHECK(queue.collect({3}) == 1);
    CHECK(destroyed == std::vector<int>{1, 2, 3});
    CHECK(queue.size() == 0);
}

TEST_CASE("RetireQueue with several queues waits for all of them") {
    RetireQueue<2> queue;
    bool destroyed = false;
    queue.push({4, 7}, [&] { destroyed = true; });

    CHECK(queue.collect({4, 6}) == 0);
    CHECK(queue.collect({3, 7}) == 0);
    CHECK_FALSE(destroyed);
    CHECK(queue.collect({4, 7}) == 1);
    CHECK(destroyed);
}

TEST_CASE("RetireQueue is conservative when values arrive out of order") {
    RetireQueue<1> queue;
    std::vector<int> destroyed;
    queue.push({5}, [&] { destroyed.push_back(5); });
    queue.push({3}, [&] { destroyed.push_back(3); });

    // The second entry is ready, but it is behind one that is not: nothing is destroyed early.
    CHECK(queue.collect({4}) == 0);
    CHECK(queue.collect({5}) == 2);
    CHECK(destroyed == std::vector<int>{5, 3});
}

TEST_CASE("RetireQueue::drain destroys everything, including objects retired by destructors") {
    RetireQueue<1> queue;
    int destroyed = 0;
    queue.push({10}, [&] {
        ++destroyed;
        queue.push({20}, [&] { ++destroyed; });
    });

    CHECK(queue.drain() == 2);
    CHECK(destroyed == 2);
    CHECK(queue.size() == 0);
}

TEST_CASE("RetireQueue destructors may retire more objects during collect") {
    RetireQueue<1> queue;
    int destroyed = 0;
    queue.push({1}, [&] {
        ++destroyed;
        queue.push({2}, [&] { ++destroyed; });
    });

    CHECK(queue.collect({1}) == 1);
    CHECK(queue.size() == 1);
    CHECK(queue.collect({2}) == 1);
    CHECK(destroyed == 2);
}

TEST_CASE("RetireQueue is thread-safe") {
    RetireQueue<1> queue;
    std::atomic<int> destroyed{0};
    constexpr int k_threads = 8;
    constexpr int k_per_thread = 1000;

    std::vector<std::thread> producers;
    for (int thread = 0; thread < k_threads; ++thread) {
        producers.emplace_back([&] {
            for (int i = 0; i < k_per_thread; ++i) {
                queue.push({static_cast<uint64_t>(i)}, [&] { destroyed.fetch_add(1, std::memory_order_relaxed); });
            }
        });
    }
    std::thread collector([&] {
        while (destroyed.load(std::memory_order_relaxed) < k_threads * k_per_thread / 2) {
            queue.collect({k_per_thread});
        }
    });
    for (std::thread& producer : producers) {
        producer.join();
    }
    collector.join();

    queue.collect({k_per_thread});
    CHECK(destroyed.load() == k_threads * k_per_thread);
    CHECK(queue.size() == 0);
}
