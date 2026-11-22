#pragma once

#include "name_table.hpp"

#include <bytee/core/assert.hpp>
#include <bytee/render/graph/render_graph.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace bytee::render::detail {

// The passes declared for one frame in execution order, with their resource accesses and attachments (flat arrays;
// each pass refers to its range).
class GraphPasses {
public:
    struct Pass {
        NameTable::Name name;
        void* object = nullptr; // the RenderPass
        RenderGraph::ExecuteFunction execute = nullptr;
        uint32_t first_access = 0;
        uint32_t access_count = 0;
        uint32_t first_color = 0;
        uint32_t color_count = 0;
        std::optional<DepthAttachment> depth;

        [[nodiscard]] bool renders() const noexcept { return color_count > 0 || depth.has_value(); }
    };

private:
    NameTable names_;
    std::vector<Pass> passes_;
    std::vector<ResourceAccess> accesses_;
    std::vector<ColorAttachment> colors_;

public:
    // Starts a pass; accesses and attachments added next belong to it. Returns its index.
    [[nodiscard]] uint32_t add(std::string_view name, void* object, RenderGraph::ExecuteFunction execute) {
        passes_.push_back({.name = names_.add(name),
                           .object = object,
                           .execute = execute,
                           .first_access = static_cast<uint32_t>(accesses_.size()),
                           .first_color = static_cast<uint32_t>(colors_.size())});
        return static_cast<uint32_t>(passes_.size() - 1);
    }

    // One state per resource per pass: the barrier before the pass moves each resource into exactly one state.
    void add_access(uint32_t index, const ResourceAccess& access) {
        Pass& pass = passes_[index];
        BYTEE_ASSERT(std::ranges::none_of(accesses(pass),
                                          [&](const ResourceAccess& other) {
                                              return other.kind == access.kind && other.resource == access.resource;
                                          }),
                     "pass '{}': resource {} is used more than once", name(pass), access.resource);
        accesses_.push_back(access);
        ++pass.access_count;
    }

    void add_color(uint32_t index, const ColorAttachment& attachment) {
        Pass& pass = passes_[index];
        BYTEE_ASSERT(pass.color_count < rhi::k_max_color_attachments, "pass '{}': more than {} color attachments",
                     name(pass), rhi::k_max_color_attachments);
        colors_.push_back(attachment);
        ++pass.color_count;
    }

    void set_depth(uint32_t index, const DepthAttachment& attachment) {
        Pass& pass = passes_[index];
        BYTEE_ASSERT(!pass.depth.has_value(), "pass '{}': more than one depth attachment", name(pass));
        pass.depth = attachment;
    }

    void clear() noexcept {
        names_.clear();
        passes_.clear();
        accesses_.clear();
        colors_.clear();
    }

    [[nodiscard]] uint32_t count() const noexcept { return static_cast<uint32_t>(passes_.size()); }
    [[nodiscard]] std::span<const Pass> all() const noexcept { return passes_; }
    [[nodiscard]] const Pass& pass(uint32_t index) const {
        BYTEE_ASSERT(index < passes_.size(), "pass index {} out of range [0, {})", index, passes_.size());
        return passes_[index];
    }
    [[nodiscard]] std::string_view name(const Pass& pass) const noexcept { return names_.get(pass.name); }
    [[nodiscard]] std::span<const ResourceAccess> accesses(const Pass& pass) const noexcept {
        return std::span{accesses_}.subspan(pass.first_access, pass.access_count);
    }
    [[nodiscard]] std::span<const ColorAttachment> colors(const Pass& pass) const noexcept {
        return std::span{colors_}.subspan(pass.first_color, pass.color_count);
    }
};

} // namespace bytee::render::detail
