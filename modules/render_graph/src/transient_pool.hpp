#pragma once

#include <bytee/core/result.hpp>
#include <bytee/render/graph/graph_types.hpp>
#include <bytee/render/rhi/device.hpp>

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace bytee::render::detail {

// Resources of one kind kept across frames: acquire() hands out a free one with the same description and usage, or
// creates one. Entries unused for `keep_frames` frames are released (deferred by the RHI: the GPU may still use them).
template <class Desc_, class Usage_, class Resource_>
class TransientCache {
public:
    using Handle = decltype(std::declval<const Resource_&>().handle());

private:
    struct Entry {
        Desc_ desc;
        Usage_ usage;
        Resource_ resource;
        uint64_t last_used_frame = 0;
        bool in_use = false;
    };

    std::vector<Entry> entries_;

public:
    template <class Create_>
        requires std::same_as<std::invoke_result_t<Create_&>, core::Result<Resource_>>
    [[nodiscard]] core::Result<Handle> acquire(const Desc_& desc, Usage_ usage, uint64_t frame, Create_&& create) {
        auto entry = std::ranges::find_if(entries_, [&](const Entry& candidate) {
            return !candidate.in_use && candidate.desc == desc && candidate.usage == usage;
        });
        if (entry == entries_.end()) {
            core::Result<Resource_> created = create();
            if (!created) {
                return std::move(created).error();
            }
            entries_.push_back({.desc = desc, .usage = usage, .resource = std::move(*created)});
            entry = std::prev(entries_.end());
        }
        entry->in_use = true;
        entry->last_used_frame = frame;
        return entry->resource.handle();
    }

    // Frees every entry for the new frame and releases those unused since before `frame - keep_frames`.
    void begin_frame(uint64_t frame, uint64_t keep_frames) {
        std::erase_if(entries_, [&](const Entry& entry) { return entry.last_used_frame + keep_frames < frame; });
        for (Entry& entry : entries_) {
            entry.in_use = false;
        }
    }

    [[nodiscard]] uint32_t size() const noexcept { return static_cast<uint32_t>(entries_.size()); }
};

// Transient textures and buffers of a graph. Reuse across frames is safe on one queue: a transient's first barrier
// (from Undefined) waits for all earlier work.
class TransientPool {
private:
    TransientCache<TextureDesc, core::Flags<rhi::TextureUsage>, rhi::Texture> textures_;
    TransientCache<BufferDesc, core::Flags<rhi::BufferUsage>, rhi::Buffer> buffers_;
    uint64_t frame_ = 0;
    uint64_t keep_frames_ = 0;

public:
    explicit TransientPool(uint64_t keep_frames) noexcept
        : keep_frames_(keep_frames) {}

    void begin_frame() {
        ++frame_;
        textures_.begin_frame(frame_, keep_frames_);
        buffers_.begin_frame(frame_, keep_frames_);
    }

    [[nodiscard]] core::Result<rhi::TextureHandle>
    acquire(rhi::Device& device, std::string_view name, const TextureDesc& desc, core::Flags<rhi::TextureUsage> usage) {
        return textures_.acquire(desc, usage, frame_, [&] {
            return device.create_texture({.dimension = desc.dimension,
                                          .extent = desc.extent,
                                          .mip_levels = desc.mip_levels,
                                          .array_layers = desc.array_layers,
                                          .format = desc.format,
                                          .usage = usage,
                                          .debug_name = name});
        });
    }

    [[nodiscard]] core::Result<rhi::BufferHandle> acquire(rhi::Device& device, std::string_view name,
                                                          const BufferDesc& desc, core::Flags<rhi::BufferUsage> usage) {
        return buffers_.acquire(desc, usage, frame_, [&] {
            return device.create_buffer({.size = desc.size, .usage = usage, .debug_name = name});
        });
    }

    [[nodiscard]] uint32_t texture_count() const noexcept { return textures_.size(); }
    [[nodiscard]] uint32_t buffer_count() const noexcept { return buffers_.size(); }
};

} // namespace bytee::render::detail
