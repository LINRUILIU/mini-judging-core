#include "mini_judge/comparator.hpp"

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
    if (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
        if (!text.empty() && text.size() >= 2 && text[text.size() - 2] == '\n') {
            return lines;
        }
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

    const auto max_lines = (expected_lines.size() > actual_lines.size()) ? expected_lines.size() : actual_lines.size();
    for (std::size_t i = 0; i < max_lines; ++i) {
        const std::string expected_line = (i < expected_lines.size()) ? expected_lines[i] : "<missing>";
        const std::string actual_line = (i < actual_lines.size()) ? actual_lines[i] : "<missing>";
        if (expected_line != actual_line) {
            result.matched = false;
            result.first_diff_line = static_cast<int>(i + 1);
            result.message = "Difference at line " + std::to_string(i + 1);
            return result;
        }
    }

    result.matched = true;
    result.first_diff_line = -1;
    result.message = "Matched";
    return result;
}

}  // namespace mini_judge
