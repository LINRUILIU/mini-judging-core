#include "mini_judge/config.hpp"

#include <Windows.h>

#include <charconv>
#include <climits>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace mini_judge {
namespace {

struct JsonValue {
    enum class Type {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object,
    };

    Type type = Type::Null;
    bool bool_value = false;
    std::string string_value;
    std::vector<JsonValue> array_value;
    std::map<std::string, JsonValue> object_value;
};

std::string ReadFileText(const std::filesystem::path& path) {
    std::unique_ptr<FILE, decltype(&fclose)> file(_wfopen(path.c_str(), L"rb"), &fclose);
    if (!file) {
        throw std::runtime_error("Failed to open config file: " + path.u8string());
    }

    std::string buffer;
    constexpr std::size_t kMaxConfigBytes = 16U * 1024U * 1024U;
    char chunk[4096];
    while (true) {
        const std::size_t read = fread(chunk, 1, sizeof(chunk), file.get());
        if (read > 0) {
            if (buffer.size() > kMaxConfigBytes - read) {
                throw std::runtime_error("Config file exceeds 16 MiB: " + path.u8string());
            }
            buffer.append(chunk, chunk + read);
        }
        if (read < sizeof(chunk)) {
            if (ferror(file.get())) {
                throw std::runtime_error("Failed to read config file: " + path.u8string());
            }
            break;
        }
    }
    FILE* const handle = file.release();
    if (fclose(handle) != 0) {
        throw std::runtime_error("Failed to close config file: " + path.u8string());
    }
    if (!buffer.empty() && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, buffer.data(),
                                                static_cast<int>(buffer.size()), nullptr, 0) == 0) {
        throw std::runtime_error("Config file is not valid UTF-8: " + path.u8string());
    }
    return buffer;
}

std::filesystem::path ResolvePath(const std::filesystem::path& base_dir, const std::string& value) {
    if (value.find('\0') != std::string::npos) {
        throw std::runtime_error("Path contains a NUL character");
    }
    std::filesystem::path raw = std::filesystem::u8path(value);
    return raw.is_absolute() ? raw : (base_dir / raw).lexically_normal();
}

bool IsBlank(const std::string& value) {
    for (unsigned char ch : value) {
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
            return false;
        }
    }
    return true;
}

void RequireDirectory(const std::filesystem::path& path, const std::string& label) {
    std::error_code error;
    if (!std::filesystem::is_directory(path, error)) {
        throw std::runtime_error(label + " must be an existing directory: " + path.u8string());
    }
}

void RequireRegularFile(const std::filesystem::path& path, const std::string& label) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
        throw std::runtime_error(label + " must be an existing regular file: " + path.u8string());
    }
}

void AppendUtf8(std::string& out, std::uint32_t codepoint) {
    if (codepoint <= 0x7f) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        out.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        out.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        out.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

class JsonParser {
public:
    explicit JsonParser(std::string_view input) : input_(input) {}

    JsonValue Parse() {
        SkipWhitespace();
        JsonValue value = ParseValue(0);
        SkipWhitespace();
        if (!AtEnd()) {
            throw std::runtime_error("Unexpected trailing JSON content");
        }
        return value;
    }

private:
    JsonValue ParseValue(unsigned depth) {
        if (depth > 64) {
            throw std::runtime_error("JSON nesting exceeds 64 levels");
        }
        if (AtEnd()) {
            throw std::runtime_error("Unexpected end of JSON input");
        }

        switch (Peek()) {
            case '{':
                return ParseObject(depth);
            case '[':
                return ParseArray(depth);
            case '"':
                return ParseString();
            case 't':
                return ParseLiteral("true", JsonValue::Type::Bool, true);
            case 'f':
                return ParseLiteral("false", JsonValue::Type::Bool, false);
            case 'n':
                return ParseLiteral("null", JsonValue::Type::Null, false);
            default:
                if (Peek() == '-' || (Peek() >= '0' && Peek() <= '9')) {
                    return ParseNumber();
                }
                break;
        }
        throw std::runtime_error("Invalid JSON value");
    }

    JsonValue ParseObject(unsigned depth) {
        Consume('{');
        JsonValue value;
        value.type = JsonValue::Type::Object;
        SkipWhitespace();
        if (TryConsume('}')) {
            return value;
        }

        while (true) {
            SkipWhitespace();
            JsonValue key = ParseString();
            SkipWhitespace();
            Consume(':');
            SkipWhitespace();
            JsonValue item = ParseValue(depth + 1);
            if (!value.object_value.emplace(key.string_value, std::move(item)).second) {
                throw std::runtime_error("Duplicate JSON object key: " + key.string_value);
            }
            SkipWhitespace();
            if (TryConsume('}')) {
                break;
            }
            Consume(',');
            SkipWhitespace();
        }
        return value;
    }

    JsonValue ParseArray(unsigned depth) {
        Consume('[');
        JsonValue value;
        value.type = JsonValue::Type::Array;
        SkipWhitespace();
        if (TryConsume(']')) {
            return value;
        }

        while (true) {
            SkipWhitespace();
            value.array_value.push_back(ParseValue(depth + 1));
            SkipWhitespace();
            if (TryConsume(']')) {
                break;
            }
            Consume(',');
            SkipWhitespace();
        }
        return value;
    }

    JsonValue ParseString() {
        Consume('"');
        JsonValue value;
        value.type = JsonValue::Type::String;

        while (!AtEnd()) {
            const char ch = Get();
            if (ch == '"') {
                return value;
            }
            if (ch == '\\') {
                if (AtEnd()) {
                    throw std::runtime_error("Invalid JSON escape");
                }
                const char escaped = Get();
                switch (escaped) {
                    case '"':
                    case '\\':
                    case '/':
                        value.string_value.push_back(escaped);
                        break;
                    case 'b':
                        value.string_value.push_back('\b');
                        break;
                    case 'f':
                        value.string_value.push_back('\f');
                        break;
                    case 'n':
                        value.string_value.push_back('\n');
                        break;
                    case 'r':
                        value.string_value.push_back('\r');
                        break;
                    case 't':
                        value.string_value.push_back('\t');
                        break;
                    case 'u': {
                        std::uint32_t codepoint = ParseHexQuad();
                        if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                            if (AtEnd() || Get() != '\\' || AtEnd() || Get() != 'u') {
                                throw std::runtime_error("Missing JSON low surrogate");
                            }
                            const std::uint32_t low = ParseHexQuad();
                            if (low < 0xdc00 || low > 0xdfff) {
                                throw std::runtime_error("Invalid JSON low surrogate");
                            }
                            codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                        } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                            throw std::runtime_error("Unexpected JSON low surrogate");
                        }
                        AppendUtf8(value.string_value, codepoint);
                        break;
                    }
                    default:
                        throw std::runtime_error("Unsupported JSON escape sequence");
                }
            } else {
                if (static_cast<unsigned char>(ch) < 0x20) {
                    throw std::runtime_error("Unescaped control character in JSON string");
                }
                value.string_value.push_back(ch);
            }
        }

        throw std::runtime_error("Unterminated JSON string");
    }

    JsonValue ParseNumber() {
        JsonValue value;
        value.type = JsonValue::Type::Number;
        const std::size_t begin = pos_;
        if (Peek() == '-') {
            ++pos_;
        }
        if (AtEnd()) {
            throw std::runtime_error("Invalid JSON number");
        }
        if (Peek() == '0') {
            ++pos_;
            if (!AtEnd() && Peek() >= '0' && Peek() <= '9') {
                throw std::runtime_error("Leading zero in JSON number");
            }
        } else if (Peek() >= '1' && Peek() <= '9') {
            do {
                ++pos_;
            } while (!AtEnd() && Peek() >= '0' && Peek() <= '9');
        } else {
            throw std::runtime_error("Invalid JSON number");
        }
        if (!AtEnd() && Peek() == '.') {
            ++pos_;
            if (AtEnd() || Peek() < '0' || Peek() > '9') {
                throw std::runtime_error("Invalid JSON fraction");
            }
            do {
                ++pos_;
            } while (!AtEnd() && Peek() >= '0' && Peek() <= '9');
        }
        if (!AtEnd() && (Peek() == 'e' || Peek() == 'E')) {
            ++pos_;
            if (!AtEnd() && (Peek() == '+' || Peek() == '-')) {
                ++pos_;
            }
            if (AtEnd() || Peek() < '0' || Peek() > '9') {
                throw std::runtime_error("Invalid JSON exponent");
            }
            do {
                ++pos_;
            } while (!AtEnd() && Peek() >= '0' && Peek() <= '9');
        }
        value.string_value = std::string(input_.substr(begin, pos_ - begin));
        return value;
    }

    JsonValue ParseLiteral(const char* token, JsonValue::Type type, bool bool_value) {
        for (const char* p = token; *p != '\0'; ++p) {
            if (AtEnd() || Get() != *p) {
                throw std::runtime_error("Invalid JSON literal");
            }
        }
        JsonValue value;
        value.type = type;
        value.bool_value = bool_value;
        return value;
    }

    void SkipWhitespace() {
        while (!AtEnd() && (Peek() == ' ' || Peek() == '\t' || Peek() == '\r' || Peek() == '\n')) {
            ++pos_;
        }
    }

    std::uint32_t ParseHexQuad() {
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            if (AtEnd()) {
                throw std::runtime_error("Incomplete JSON Unicode escape");
            }
            const char ch = Get();
            value <<= 4;
            if (ch >= '0' && ch <= '9') {
                value |= static_cast<std::uint32_t>(ch - '0');
            } else if (ch >= 'a' && ch <= 'f') {
                value |= static_cast<std::uint32_t>(ch - 'a' + 10);
            } else if (ch >= 'A' && ch <= 'F') {
                value |= static_cast<std::uint32_t>(ch - 'A' + 10);
            } else {
                throw std::runtime_error("Invalid JSON Unicode escape");
            }
        }
        return value;
    }

    void Consume(char expected) {
        if (AtEnd() || Get() != expected) {
            throw std::runtime_error(std::string("Expected JSON token: ") + expected);
        }
    }

    bool TryConsume(char expected) {
        if (!AtEnd() && Peek() == expected) {
            ++pos_;
            return true;
        }
        return false;
    }

    char Peek() const { return input_[pos_]; }
    char Get() { return input_[pos_++]; }
    bool AtEnd() const { return pos_ >= input_.size(); }

    std::string_view input_;
    std::size_t pos_ = 0;
};

const JsonValue& RequireObjectField(const JsonValue& object, const std::string& key) {
    if (object.type != JsonValue::Type::Object) {
        throw std::runtime_error("Expected JSON object");
    }
    const auto it = object.object_value.find(key);
    if (it == object.object_value.end()) {
        throw std::runtime_error("Missing required field: " + key);
    }
    return it->second;
}

std::string RequireString(const JsonValue& value, const std::string& label) {
    if (value.type != JsonValue::Type::String) {
        throw std::runtime_error("Expected string field: " + label);
    }
    return value.string_value;
}

std::string RequireNonBlankString(const JsonValue& value, const std::string& label) {
    std::string result = RequireString(value, label);
    if (IsBlank(result)) {
        throw std::runtime_error("Field must not be empty: " + label);
    }
    return result;
}

std::string RequireIdentifier(const JsonValue& value, const std::string& label) {
    std::string result = RequireNonBlankString(value, label);
    for (unsigned char ch : result) {
        if (ch < 0x20 || ch == 0x7f) {
            throw std::runtime_error("Identifier contains a control character: " + label);
        }
    }
    return result;
}

int RequireInt(const JsonValue& value, const std::string& label) {
    if (value.type != JsonValue::Type::Number) {
        throw std::runtime_error("Expected integer field: " + label);
    }
    int parsed = 0;
    const char* begin = value.string_value.data();
    const char* end = begin + value.string_value.size();
    const auto conversion = std::from_chars(begin, end, parsed);
    if (conversion.ec != std::errc{} || conversion.ptr != end) {
        throw std::runtime_error("Expected bounded integer field: " + label);
    }
    return parsed;
}

std::uint64_t RequireUInt64(const JsonValue& value, const std::string& label) {
    if (value.type != JsonValue::Type::Number) {
        throw std::runtime_error("Expected integer field: " + label);
    }
    std::uint64_t parsed = 0;
    const char* begin = value.string_value.data();
    const char* end = begin + value.string_value.size();
    const auto conversion = std::from_chars(begin, end, parsed);
    if (conversion.ec != std::errc{} || conversion.ptr != end) {
        throw std::runtime_error("Expected bounded unsigned integer field: " + label);
    }
    return parsed;
}

const std::vector<JsonValue>& RequireArray(const JsonValue& value, const std::string& label) {
    if (value.type != JsonValue::Type::Array) {
        throw std::runtime_error("Expected array field: " + label);
    }
    return value.array_value;
}

}  // namespace

CompareMode ParseCompareMode(const std::string& raw) {
    if (raw == "exact") {
        return CompareMode::Exact;
    }
    if (raw == "trim-right") {
        return CompareMode::TrimRight;
    }
    if (raw == "ignore-trailing-blank-lines") {
        return CompareMode::IgnoreTrailingBlankLines;
    }
    throw std::runtime_error("Unsupported compare mode: " + raw);
}

ProblemConfig LoadConfig(const std::filesystem::path& config_path) {
    const auto config_text = ReadFileText(config_path);
    const JsonValue root = JsonParser(config_text).Parse();

    ProblemConfig config;
    config.config_path = std::filesystem::absolute(config_path);
    const auto base_dir = config.config_path.parent_path();

    config.problem_id = RequireIdentifier(RequireObjectField(root, "problem_id"), "problem_id");
    config.time_limit_ms = RequireInt(RequireObjectField(root, "time_limit_ms"), "time_limit_ms");
    config.max_output_bytes = RequireUInt64(RequireObjectField(root, "max_output_bytes"), "max_output_bytes");
    config.compare_mode = ParseCompareMode(RequireString(RequireObjectField(root, "compare_mode"), "compare_mode"));
    if (config.time_limit_ms < 1 || config.time_limit_ms > 3600000) {
        throw std::runtime_error("time_limit_ms must be between 1 and 3600000");
    }
    constexpr std::uint64_t kMaxOutputBytes = 64ULL * 1024ULL * 1024ULL;
    if (config.max_output_bytes < 1 || config.max_output_bytes > kMaxOutputBytes) {
        throw std::runtime_error("max_output_bytes must be between 1 and 67108864");
    }

    const JsonValue& paths = RequireObjectField(root, "paths");
    config.program_dir = ResolvePath(base_dir, RequireNonBlankString(RequireObjectField(paths, "program_dir"), "paths.program_dir"));
    config.test_dir = ResolvePath(base_dir, RequireNonBlankString(RequireObjectField(paths, "test_dir"), "paths.test_dir"));
    const auto answer_dir_field = paths.object_value.find("answer_dir");
    if (answer_dir_field == paths.object_value.end()) {
        config.answer_dir = config.test_dir;
    } else {
        config.answer_dir = ResolvePath(base_dir, RequireNonBlankString(answer_dir_field->second, "paths.answer_dir"));
    }
    config.report_dir = ResolvePath(base_dir, RequireNonBlankString(RequireObjectField(paths, "report_dir"), "paths.report_dir"));

    RequireDirectory(config.program_dir, "Program directory");
    RequireDirectory(config.test_dir, "Test directory");
    RequireDirectory(config.answer_dir, "Answer directory");

    const auto& programs = RequireArray(RequireObjectField(root, "programs"), "programs");
    if (programs.empty()) {
        throw std::runtime_error("Program list must not be empty");
    }
    std::set<std::string> program_ids;
    for (const auto& item : programs) {
        ProgramEntry entry;
        entry.id = RequireIdentifier(RequireObjectField(item, "id"), "programs[].id");
        entry.exe = RequireNonBlankString(RequireObjectField(item, "exe"), "programs[].exe");
        if (!program_ids.insert(entry.id).second) {
            throw std::runtime_error("Duplicate program id: " + entry.id);
        }
        entry.exe_path = ResolvePath(config.program_dir, entry.exe);
        RequireRegularFile(entry.exe_path, "Program executable");
        config.programs.push_back(std::move(entry));
    }

    const auto& tests = RequireArray(RequireObjectField(root, "tests"), "tests");
    if (tests.empty()) {
        throw std::runtime_error("Test list must not be empty");
    }
    std::set<std::string> test_ids;
    int total_possible_score = 0;
    for (const auto& item : tests) {
        TestCase test;
        test.id = RequireIdentifier(RequireObjectField(item, "id"), "tests[].id");
        test.input = RequireNonBlankString(RequireObjectField(item, "input"), "tests[].input");
        test.answer = RequireNonBlankString(RequireObjectField(item, "answer"), "tests[].answer");
        if (!test_ids.insert(test.id).second) {
            throw std::runtime_error("Duplicate test id: " + test.id);
        }
        test.score = RequireInt(RequireObjectField(item, "score"), "tests[].score");
        if (test.score < 0 || test.score > INT_MAX - total_possible_score) {
            throw std::runtime_error("Test scores must be nonnegative and total at most INT_MAX");
        }
        total_possible_score += test.score;
        test.input_path = ResolvePath(config.test_dir, test.input);
        test.answer_path = ResolvePath(config.answer_dir, test.answer);
        RequireRegularFile(test.input_path, "Test input");
        RequireRegularFile(test.answer_path, "Test answer");
        config.tests.push_back(std::move(test));
    }

    if (config.programs.size() > static_cast<std::size_t>(INT_MAX) / config.tests.size()) {
        throw std::runtime_error("Program and test count exceeds INT_MAX");
    }

    return config;
}

std::string ToString(CompareMode mode) {
    switch (mode) {
        case CompareMode::Exact:
            return "exact";
        case CompareMode::TrimRight:
            return "trim-right";
        case CompareMode::IgnoreTrailingBlankLines:
            return "ignore-trailing-blank-lines";
    }
    return "unknown";
}

std::string ToString(CaseStatus status) {
    switch (status) {
        case CaseStatus::AC:
            return "AC";
        case CaseStatus::WA:
            return "WA";
        case CaseStatus::TLE:
            return "TLE";
        case CaseStatus::RE:
            return "RE";
        case CaseStatus::OLE:
            return "OLE";
        case CaseStatus::SE:
            return "SE";
    }
    return "SE";
}

}  // namespace mini_judge
