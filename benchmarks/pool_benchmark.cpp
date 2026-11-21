// Micro-benchmark of core::Pool against common alternatives. Run a Release build:
//   cmake --preset release && cmake --build --preset release && ./build/release/bin/bytee_pool_benchmark
// Numbers are indicative (single machine, warm caches); compare relative, not absolute, values.

#include <bytee/core/pool.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <format>
#include <numeric>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct BenchTag;

// A typical small backend record (handle-sized fields, a few flags).
struct Record {
    uint64_t native_handle = 0;
    uint64_t allocation = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;
    uint32_t flags = 0;
};

constexpr uint32_t k_count = 1u << 16; // 65536 live objects
constexpr int k_repetitions = 20;

using Clock = std::chrono::steady_clock;

// Keeps results observable so the optimizer cannot delete the measured work.
volatile uint64_t g_sink = 0;

template <class Function_>
double nanoseconds_per_operation(Function_&& function, uint32_t operations) {
    double best = 1e300;
    for (int repetition = 0; repetition < k_repetitions; ++repetition) {
        const auto start = Clock::now();
        function();
        const std::chrono::duration<double, std::nano> elapsed = Clock::now() - start;
        best = std::min(best, elapsed.count() / operations);
    }
    return best;
}

void report(std::string_view name, double pool, double map, double vector) {
    const std::string line =
        std::format("{:<34} pool {:6.2f} ns | unordered_map {:6.2f} ns | vector {:6.2f} ns\n", name, pool, map, vector);
    std::fputs(line.c_str(), stdout);
}

} // namespace

int main() {
    using Pool = bytee::core::Pool<Record, BenchTag>;

    // Random lookup order, identical for every container.
    std::vector<uint32_t> order(k_count);
    std::iota(order.begin(), order.end(), 0u);
    std::shuffle(order.begin(), order.end(), std::mt19937{42});

    // --- insert + remove (churn) ---
    const double pool_churn = nanoseconds_per_operation(
        [] {
            Pool pool{k_count};
            std::vector<Pool::HandleType> handles(k_count);
            for (uint32_t i = 0; i < k_count; ++i) {
                handles[i] = pool.insert(Record{.native_handle = i});
            }
            for (const auto handle : handles) {
                pool.remove(handle);
            }
            g_sink = g_sink + pool.size();
        },
        2 * k_count);
    const double map_churn = nanoseconds_per_operation(
        [] {
            std::unordered_map<uint64_t, Record> map;
            for (uint32_t i = 0; i < k_count; ++i) {
                map.emplace(i, Record{.native_handle = i});
            }
            for (uint32_t i = 0; i < k_count; ++i) {
                map.erase(i);
            }
            g_sink = g_sink + map.size();
        },
        2 * k_count);
    const double vector_churn = nanoseconds_per_operation(
        [] {
            std::vector<Record> vector;
            for (uint32_t i = 0; i < k_count; ++i) {
                vector.push_back(Record{.native_handle = i});
            }
            vector.clear();
            g_sink = g_sink + vector.size();
        },
        2 * k_count);
    report("insert + remove", pool_churn, map_churn, vector_churn);

    // --- random lookups ---
    Pool pool{k_count};
    std::unordered_map<uint64_t, Record> map;
    std::vector<Record> vector;
    std::vector<Pool::HandleType> handles(k_count);
    for (uint32_t i = 0; i < k_count; ++i) {
        handles[i] = pool.insert(Record{.native_handle = i});
        map.emplace((uint64_t{handles[i].generation()} << 32u) | i, Record{.native_handle = i});
        vector.push_back(Record{.native_handle = i});
    }
    std::vector<Pool::HandleType> pool_keys(k_count);
    std::vector<uint64_t> map_keys(k_count);
    for (uint32_t i = 0; i < k_count; ++i) {
        pool_keys[i] = handles[order[i]];
        map_keys[i] = (uint64_t{handles[order[i]].generation()} << 32u) | order[i];
    }

    const double pool_get = nanoseconds_per_operation(
        [&] {
            uint64_t sum = 0;
            for (const auto handle : pool_keys) {
                sum += pool.get(handle).native_handle;
            }
            g_sink = g_sink + sum;
        },
        k_count);
    const double pool_try_get = nanoseconds_per_operation(
        [&] {
            uint64_t sum = 0;
            for (const auto handle : pool_keys) {
                sum += pool.try_get(handle)->native_handle;
            }
            g_sink = g_sink + sum;
        },
        k_count);
    const double map_get = nanoseconds_per_operation(
        [&] {
            uint64_t sum = 0;
            for (const uint64_t key : map_keys) {
                sum += map.find(key)->second.native_handle;
            }
            g_sink = g_sink + sum;
        },
        k_count);
    const double vector_get = nanoseconds_per_operation(
        [&] {
            uint64_t sum = 0;
            for (const uint32_t index : order) {
                sum += vector[index].native_handle;
            }
            g_sink = g_sink + sum;
        },
        k_count);
    report("random lookup (get, unchecked)", pool_get, map_get, vector_get);
    report("random lookup (try_get, checked)", pool_try_get, map_get, vector_get);
    return 0;
}
