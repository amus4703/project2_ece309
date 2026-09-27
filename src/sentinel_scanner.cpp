// src/sentinel_scanner.cpp
// Student implementation for ECE 309 Project 2.

#include "core/sentinel_scanner.h"

#include <utility>

SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(std::move(sentinel)) {}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    if (sentinel_found_) return {"", true};

    // An empty sentinel is not useful to the harness. Treat it as disabled
    // rather than making every chunk immediately look like a stop marker.
    if (sentinel_.empty()) return {std::string(chunk), false};

    std::string safe_text;
    safe_text.reserve(chunk.size());

    for (char character : chunk) {
        pending_.push_back(character);

        if (pending_ == sentinel_) {
            pending_.clear();
            sentinel_found_ = true;
            return {std::move(safe_text), true};
        }

        // Keep only the suffix that could still grow into the sentinel. Any
        // discarded leading character is conclusively safe to display.
        while (!pending_.empty() &&
               sentinel_.compare(0, pending_.size(), pending_) != 0) {
            safe_text.push_back(pending_.front());
            pending_.erase(0, 1);
        }
    }

    return {std::move(safe_text), false};
}

SentinelScanner::Out SentinelScanner::flush() {
    if (sentinel_found_) return {"", true};

    std::string safe_text = std::move(pending_);
    pending_.clear();
    return {std::move(safe_text), false};
}
