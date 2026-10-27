#ifndef MH_TOOLS_TOOL_REGISTRY_H
#define MH_TOOLS_TOOL_REGISTRY_H

#include "tools/tool.h"
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class ToolRegistry {
public:
    ToolRegistry();
    ~ToolRegistry();
    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;

    bool add(std::unique_ptr<Tool> tool);
    Tool* find(std::string_view name) const;
    std::size_t size() const noexcept;
    void for_each(const std::function<void(const Tool&)>& fn) const;

    // ---- Instrumentation ----
    std::size_t bucket_count() const noexcept;
    double load_factor() const noexcept;
    std::size_t max_probe_length() const;
    static std::size_t hash(std::string_view key) noexcept;

private:
    std::vector<std::vector<std::unique_ptr<Tool>>> buckets_;
    std::size_t num_tools_ = 0;
    static constexpr double kMaxLoadFactor = 0.75;
    
    void rehash(std::size_t new_bucket_count);
};

#endif // MH_TOOLS_TOOL_REGISTRY_H