#include "core/sentinel_scanner.h"

SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(std::move(sentinel)), pending_("") {}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    std::string text = pending_ + std::string(chunk);
    pending_.clear();

    if (sentinel_.empty()) {
        return Out{text, false};
    }

    std::size_t pos = text.find(sentinel_);
    if (pos != std::string::npos) {
        std::string safe_text = text.substr(0, pos);
        return Out{safe_text, true};
    }

    std::size_t max_keep = sentinel_.size() - 1;
    if (text.size() > max_keep) {
        std::size_t safe_len = text.size() - max_keep;
        std::string safe_text = text.substr(0, safe_len);
        pending_ = text.substr(safe_len);
        return Out{safe_text, false};
    } else {
        pending_ = text;
        return Out{"", false};
    }
}

SentinelScanner::Out SentinelScanner::flush() {
    std::string safe_text = std::move(pending_);
    pending_.clear();
    return Out{safe_text, false};
}