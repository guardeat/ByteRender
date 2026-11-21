#include "common/retire_list.hpp"

#include <doctest/doctest.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

using bytee::render::rhi::detail::is_reached;
using bytee::render::rhi::detail::RetireList;

TEST_CASE("is_reached requires every queue to reach its value") {
    CHECK(is_reached<2>({0, 0}, {0, 0}));
    CHECK(is_reached<2>({3, 5}, {3, 5}));
    CHECK(is_reached<2>({3, 5}, {4, 9}));
    CHECK_FALSE(is_reached<2>({3, 5}, {2, 9}));
    CHECK_FALSE(is_reached<2>({3, 5}, {9, 4}));
}

TEST_CASE("Items are destroyed once their values complete, in order") {
    RetireList<int, 1> list;
    std::vector<int> destroyed;
    const auto destroy = [&](int item) { destroyed.push_back(item); };

    list.push({1}, 10);
    list.push({1}, 11);
    list.push({3}, 12);
    CHECK(list.size() == 3);

    CHECK(list.collect({0}, destroy) == 0);
    CHECK(list.collect({2}, destroy) == 2);
    CHECK(destroyed == std::vector<int>{10, 11});
    CHECK(list.collect({3}, destroy) == 1);
    CHECK(destroyed == std::vector<int>{10, 11, 12});
    CHECK(list.size() == 0);
}

TEST_CASE("Several queues must all reach their values") {
    RetireList<int, 2> list;
    int destroyed = 0;
    const auto destroy = [&](int) { ++destroyed; };
    list.push({4, 7}, 1);

    CHECK(list.collect({4, 6}, destroy) == 0);
    CHECK(list.collect({3, 7}, destroy) == 0);
    CHECK(list.collect({4, 7}, destroy) == 1);
    CHECK(destroyed == 1);
}

TEST_CASE("Out-of-order values are handled conservatively") {
    RetireList<int, 1> list;
    std::vector<int> destroyed;
    const auto destroy = [&](int item) { destroyed.push_back(item); };
    list.push({5}, 5);
    list.push({3}, 3);

    CHECK(list.collect({4}, destroy) == 0); // the ready item waits behind one that is not: never early
    CHECK(list.collect({5}, destroy) == 2);
    CHECK(destroyed == std::vector<int>{5, 3});
}

TEST_CASE("Destroy functions may retire more items") {
    RetireList<int, 1> list;
    std::vector<int> destroyed;
    list.push({1}, 1);

    const auto destroy = [&](int item) {
        destroyed.push_back(item);
        if (item == 1) {
            list.push({2}, 2);
        }
    };
    CHECK(list.collect({1}, destroy) == 1);
    CHECK(list.size() == 1);
    CHECK(list.drain(destroy) == 1);
    CHECK(destroyed == std::vector<int>{1, 2});
}

TEST_CASE("Items can be move-only") {
    RetireList<std::unique_ptr<int>, 1> list;
    list.push({1}, std::make_unique<int>(42));
    int seen = 0;
    CHECK(list.collect({1}, [&](std::unique_ptr<int>& item) { seen = *item; }) == 1);
    CHECK(seen == 42);
}

TEST_CASE("RetireList is thread-safe") {
    RetireList<int, 1> list;
    std::atomic<int> destroyed{0};
    constexpr int k_threads = 8;
    constexpr int k_per_thread = 2000;
    const auto destroy = [&](int) { destroyed.fetch_add(1, std::memory_order_relaxed); };

    std::vector<std::thread> producers;
    for (int thread = 0; thread < k_threads; ++thread) {
        producers.emplace_back([&] {
            for (int i = 0; i < k_per_thread; ++i) {
                list.push({static_cast<uint64_t>(i)}, i);
            }
        });
    }
    std::thread collector([&] {
        while (destroyed.load(std::memory_order_relaxed) < k_threads * k_per_thread / 2) {
            list.collect({k_per_thread}, destroy);
        }
    });
    for (std::thread& producer : producers) {
        producer.join();
    }
    collector.join();
    list.collect({k_per_thread}, destroy);
    CHECK(destroyed.load() == k_threads * k_per_thread);
    CHECK(list.size() == 0);
}
