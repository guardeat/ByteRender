#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace bytee::render::detail {

// Names stored back to back in one string: a frame's names cost no allocation once the table has grown, and clear()
// keeps the capacity. Views returned by get() are valid until the next add() or clear().
class NameTable {
public:
    struct Name {
        uint32_t offset = 0;
        uint32_t size = 0;
    };

private:
    std::string text_;

public:
    [[nodiscard]] Name add(std::string_view name) {
        const Name added{.offset = static_cast<uint32_t>(text_.size()), .size = static_cast<uint32_t>(name.size())};
        text_.append(name);
        return added;
    }

    [[nodiscard]] std::string_view get(Name name) const noexcept {
        return std::string_view{text_}.substr(name.offset, name.size);
    }

    void clear() noexcept { text_.clear(); }
};

} // namespace bytee::render::detail
