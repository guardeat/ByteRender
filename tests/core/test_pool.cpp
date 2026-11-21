#include <bytee/core/pool.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <vector>

namespace {

struct ItemTag;
using ItemPool = bytee::core::Pool<std::string, ItemTag>;
using ItemHandle = ItemPool::HandleType;

// Counts live instances to verify construction/destruction pairing.
struct Counted {
    int* alive;
    int value;
    Counted(int* counter, int v)
        : alive(counter)
        , value(v) {
        ++*alive;
    }
    Counted(const Counted&) = delete;
    Counted& operator=(const Counted&) = delete;
    ~Counted() { --*alive; }
};

} // namespace

TEST_CASE("Insert, look up and remove") {
    ItemPool pool{16};
    const ItemHandle a = pool.insert("alpha");
    const ItemHandle b = pool.insert(5, 'b');
    REQUIRE(a.is_valid());
    REQUIRE(b.is_valid());
    CHECK(pool.size() == 2);
    CHECK(pool.get(a) == "alpha");
    CHECK(pool.get(b) == "bbbbb");

    CHECK(pool.remove(a));
    CHECK_FALSE(pool.contains(a));
    CHECK(pool.try_get(a) == nullptr);
    CHECK(pool.size() == 1);
    CHECK(pool.get(b) == "bbbbb");
}

TEST_CASE("Reused slots get a new generation, so stale handles stay invalid") {
    ItemPool pool{4};
    const ItemHandle old_handle = pool.insert("old");
    REQUIRE(pool.remove(old_handle));

    const ItemHandle new_handle = pool.insert("new");
    CHECK(new_handle.index() == old_handle.index()); // freed slot reused (LIFO)
    CHECK(new_handle.generation() != old_handle.generation());
    CHECK_FALSE(pool.contains(old_handle));
    CHECK(pool.try_get(old_handle) == nullptr);
    CHECK(pool.get(new_handle) == "new");
}

TEST_CASE("Invalid and forged handles are rejected") {
    ItemPool pool{4};
    static_cast<void>(pool.insert("x"));
    CHECK_FALSE(pool.contains(ItemHandle{}));
    CHECK_FALSE(pool.contains(ItemHandle{0, 2}));  // even generation: never alive
    CHECK_FALSE(pool.contains(ItemHandle{0, 3}));  // wrong generation
    CHECK_FALSE(pool.contains(ItemHandle{3, 1}));  // never used slot
    CHECK_FALSE(pool.contains(ItemHandle{99, 1})); // beyond capacity
}

TEST_CASE("A full pool returns an invalid handle") {
    ItemPool pool{2};
    CHECK(pool.insert("1").is_valid());
    CHECK(pool.insert("2").is_valid());
    CHECK_FALSE(pool.insert("3").is_valid());
    CHECK(pool.size() == 2);
}

TEST_CASE("Values keep their address while the pool grows") {
    ItemPool pool{4 * ItemPool::k_chunk_size};
    const ItemHandle first = pool.insert("first");
    const std::string* address = &pool.get(first);
    std::vector<ItemHandle> others;
    for (uint32_t i = 0; i < 3 * ItemPool::k_chunk_size; ++i) {
        others.push_back(pool.insert(std::to_string(i)));
    }
    CHECK(&pool.get(first) == address);
    CHECK(pool.get(others.back()) == std::to_string(3 * ItemPool::k_chunk_size - 1));
}

TEST_CASE("Values are destroyed on remove, clear and pool destruction") {
    int alive = 0;
    {
        bytee::core::Pool<Counted, ItemTag> pool{8};
        const auto a = pool.insert(&alive, 1);
        static_cast<void>(pool.insert(&alive, 2));
        static_cast<void>(pool.insert(&alive, 3));
        CHECK(alive == 3);

        pool.remove(a);
        CHECK(alive == 2);

        pool.clear();
        CHECK(alive == 0);
        CHECK(pool.empty());

        static_cast<void>(pool.insert(&alive, 4));
        CHECK(alive == 1);
    }
    CHECK(alive == 0);
}

TEST_CASE("Move-only values and moving pools") {
    bytee::core::Pool<std::unique_ptr<int>, ItemTag> pool{4};
    const auto handle = pool.insert(std::make_unique<int>(42));
    CHECK(*pool.get(handle) == 42);

    auto moved = std::move(pool);
    CHECK(moved.size() == 1);
    CHECK(*moved.get(handle) == 42);
    CHECK(pool.size() == 0); // NOLINT(bugprone-use-after-move): checking the moved-from state
}

TEST_CASE("for_each visits live values in slot order") {
    ItemPool pool{8};
    const ItemHandle a = pool.insert("a");
    const ItemHandle b = pool.insert("b");
    const ItemHandle c = pool.insert("c");
    pool.remove(b);

    std::vector<ItemHandle> visited;
    std::string text;
    pool.for_each([&](ItemHandle handle, std::string& value) {
        visited.push_back(handle);
        text += value;
    });
    CHECK(visited == std::vector<ItemHandle>{a, c});
    CHECK(text == "ac");
}
