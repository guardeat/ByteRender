#pragma once

#include "name_table.hpp"

#include <bytee/core/assert.hpp>
#include <bytee/render/graph/graph_types.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace bytee::render::detail {

// The textures and buffers declared for one frame, exactly as declared (what compile() derives from them lives in
// GraphSchedule).
class GraphResources {
public:
    struct Texture {
        NameTable::Name name;
        TextureDesc desc;
        std::optional<rhi::TextureHandle> imported; // empty for transients
        ImportedStates states;
    };

    struct Buffer {
        NameTable::Name name;
        BufferDesc desc;
        std::optional<rhi::BufferHandle> imported; // empty for transients
        ImportedStates states;
    };

private:
    NameTable names_;
    std::vector<Texture> textures_;
    std::vector<Buffer> buffers_;

public:
    [[nodiscard]] TextureRef add_texture(std::string_view name, const TextureDesc& desc,
                                         std::optional<rhi::TextureHandle> imported = {}, ImportedStates states = {}) {
        textures_.push_back({.name = names_.add(name), .desc = desc, .imported = imported, .states = states});
        return TextureRef{static_cast<uint32_t>(textures_.size() - 1)};
    }

    [[nodiscard]] BufferRef add_buffer(std::string_view name, const BufferDesc& desc,
                                       std::optional<rhi::BufferHandle> imported = {}, ImportedStates states = {}) {
        buffers_.push_back({.name = names_.add(name), .desc = desc, .imported = imported, .states = states});
        return BufferRef{static_cast<uint32_t>(buffers_.size() - 1)};
    }

    void clear() noexcept {
        names_.clear();
        textures_.clear();
        buffers_.clear();
    }

    [[nodiscard]] uint32_t texture_count() const noexcept { return static_cast<uint32_t>(textures_.size()); }
    [[nodiscard]] uint32_t buffer_count() const noexcept { return static_cast<uint32_t>(buffers_.size()); }
    [[nodiscard]] uint32_t count(ResourceKind kind) const noexcept {
        return kind == ResourceKind::Texture ? texture_count() : buffer_count();
    }

    [[nodiscard]] const Texture& texture(uint32_t index) const {
        BYTEE_ASSERT(index < textures_.size(), "invalid TextureRef {}", index);
        return textures_[index];
    }
    [[nodiscard]] const Buffer& buffer(uint32_t index) const {
        BYTEE_ASSERT(index < buffers_.size(), "invalid BufferRef {}", index);
        return buffers_[index];
    }

    [[nodiscard]] std::string_view name(const Texture& texture) const noexcept { return names_.get(texture.name); }
    [[nodiscard]] std::string_view name(const Buffer& buffer) const noexcept { return names_.get(buffer.name); }
    [[nodiscard]] std::string_view name(ResourceKind kind, uint32_t index) const {
        return kind == ResourceKind::Texture ? name(texture(index)) : name(buffer(index));
    }
    [[nodiscard]] bool is_imported(ResourceKind kind, uint32_t index) const {
        return kind == ResourceKind::Texture ? texture(index).imported.has_value() : buffer(index).imported.has_value();
    }
};

} // namespace bytee::render::detail
