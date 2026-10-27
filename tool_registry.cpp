#include "tools/tool_registry.h"
#include <algorithm>
#include <cstdint>

ToolRegistry::ToolRegistry() : buckets_(16) {}

ToolRegistry::~ToolRegistry() = default;

std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : key) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return static_cast<std::size_t>(h);
}

std::size_t ToolRegistry::bucket_count() const noexcept {
    return buckets_.size();
}

std::size_t ToolRegistry::size() const noexcept {
    return num_tools_;
}

double ToolRegistry::load_factor() const noexcept {
    if (buckets_.empty()) return 0.0;
    return static_cast<double>(num_tools_) / static_cast<double>(buckets_.size());
}

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t max_len = 0;
    for (const auto& bucket : buckets_) {
        if (bucket.size() > max_len) {
            max_len = bucket.size();
        }
    }
    return max_len;
}

Tool* ToolRegistry::find(std::string_view name) const {
    if (buckets_.empty()) return nullptr;
    std::size_t idx = hash(name) % buckets_.size();
    for (const auto& tool_ptr : buckets_[idx]) {
        if (tool_ptr && tool_ptr->name() == name) {
            return tool_ptr.get();
        }
    }
    return nullptr;
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (!tool) return false;
    
    if (find(tool->name()) != nullptr) {
        return false;
    }

    if ((static_cast<double>(num_tools_ + 1) / static_cast<double>(buckets_.size())) > kMaxLoadFactor) {
        rehash(buckets_.size() * 2);
    }

    std::size_t idx = hash(tool->name()) % buckets_.size();
    buckets_[idx].push_back(std::move(tool));
    num_tools_++;
    return true;
}

void ToolRegistry::rehash(std::size_t new_bucket_count) {
    if (new_bucket_count == 0) return;
    std::vector<std::vector<std::unique_ptr<Tool>>> new_buckets(new_bucket_count);
    for (auto& bucket : buckets_) {
        for (auto& tool_ptr : bucket) {
            if (tool_ptr) {
                std::size_t new_idx = hash(tool_ptr->name()) % new_bucket_count;
                new_buckets[new_idx].push_back(std::move(tool_ptr));
            }
        }
    }
    buckets_ = std::move(new_buckets);
}

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const auto& bucket : buckets_) {
        for (const auto& tool_ptr : bucket) {
            if (tool_ptr) {
                fn(*tool_ptr);
            }
        }
    }
}