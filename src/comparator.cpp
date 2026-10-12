#include "mini_judge/comparator.hpp"

#include <algorithm>
#include <sstream>
#include <vector>

namespace mini_judge {
namespace {

std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::stringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

std::string TrimRight(std::string value) {
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) {
        value.pop_back();
    }
    return value;
}

void RemoveTrailingBlankLines(std::vector<std::string>& lines) {
    while (!lines.empty() && lines.back().empty()) {
        lines.pop_back();
    }
}

}  // namespace

CompareResult CompareText(CompareMode mode, const std::string& expected, const std::string& actual) {
    CompareResult result;

    if (mode == CompareMode::Exact) {
        result.matched = (expected == actual);
        if (!result.matched) {
            result.first_diff_line = 1;
            result.message = "Exact output mismatch";
        }
        return result;
    }

    auto expected_lines = SplitLines(expected);
    auto actual_lines = SplitLines(actual);

    if (mode == CompareMode::TrimRight) {
        for (auto& line : expected_lines) {
            line = TrimRight(line);
        }
        for (auto& line : actual_lines) {
            line = TrimRight(line);
        }
    } else if (mode == CompareMode::IgnoreTrailingBlankLines) {
        RemoveTrailingBlankLines(expected_lines);
        RemoveTrailingBlankLines(actual_lines);
    }

    // Compare only lines that exist on both sides. A literal "<missing>" line is data,
    // never a sentinel for an absent line.
    const auto common_lines = std::min(expected_lines.size(), actual_lines.size());
    for (std::size_t i = 0; i < common_lines; ++i) {
        if (expected_lines[i] != actual_lines[i]) {
            result.matched = false;
            result.first_diff_line = static_cast<int>(i + 1);
            result.message = "Difference at line " + std::to_string(i + 1);
            return result;
        }
    }

    if (expected_lines.size() != actual_lines.size()) {
        result.matched = false;
        result.first_diff_line = static_cast<int>(common_lines + 1);
        result.message = "Difference at line " + std::to_string(common_lines + 1);
        return result;
    }

    result.matched = true;
    result.first_diff_line = -1;
    result.message = "Matched";
    return result;
}

}  // namespace mini_judge
