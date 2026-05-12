#include "mini_judge/config.hpp"

#include <Windows.h>

#include <cctype>
#include <cstdio>
#include <map>
#include <sstream>
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
    FILE* file = _wfopen(path.c_str(), L"rb");
    if (file == nullptr) {
        throw std::runtime_error("Failed to open config file: " + path.string());
    }

    std::string buffer;
    char chunk[4096];
    while (true) {
        const std::size_t read = fread(chunk, 1, sizeof(chunk), file);
        if (read > 0) {
            buffer.append(chunk, chunk + read);
        }
        if (read < sizeof(chunk)) {
            break;
        }
    }
    fclose(file);
    return buffer;
}

std::filesystem::path ResolvePath(const std::filesystem::path& base_dir, const std::string& value) {
    std::filesystem::path raw = std::filesystem::u8path(value);
    return raw.is_absolute() ? raw : (base_dir / raw).lexically_normal();
}

bool PathExists(const std::filesystem::path& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

class JsonParser {
public:
    explicit JsonParser(std::string_view input) : input_(input) {}

    JsonValue Parse() {
        SkipWhitespace();
        JsonValue value = ParseValue();
        SkipWhitespace();
        if (!AtEnd()) {
            throw std::runtime_error("Unexpected trailing JSON content");
        }
        return value;
    }

private:
    JsonValue ParseValue() {
        if (AtEnd()) {
            throw std::runtime_error("Unexpected end of JSON input");
        }

        switch (Peek()) {
            case '{':
                return ParseObject();
            case '[':
                return ParseArray();
            case '"':
                return ParseString();
            case 't':
                return ParseLiteral("true", JsonValue::Type::Bool, true);
            case 'f':
                return ParseLiteral("false", JsonValue::Type::Bool, false);
            case 'n':
                return ParseLiteral("null", JsonValue::Type::Null, false);
            default:
                if (Peek() == '-' || std::isdigit(static_cast<unsigned char>(Peek()))) {
                    return ParseNumber();
                }
                break;
        }
        throw std::runtime_error("Invalid JSON value");
    }

    JsonValue ParseObject() {
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
            JsonValue item = ParseValue();
            value.object_value.emplace(key.string_value, std::move(item));
            SkipWhitespace();
            if (TryConsume('}')) {
                break;
            }
            Consume(',');
            SkipWhitespace();
        }
        return value;
    }

    JsonValue ParseArray() {
        Consume('[');
        JsonValue value;
        value.type = JsonValue::Type::Array;
        SkipWhitespace();
        if (TryConsume(']')) {
            return value;
        }

        while (true) {
            SkipWhitespace();
            value.array_value.push_back(ParseValue());
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
                    default:
                        throw std::runtime_error("Unsupported JSON escape sequence");
                }
            } else {
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
        while (!AtEnd() && std::isdigit(static_cast<unsigned char>(Peek()))) {
            ++pos_;
        }
        if (!AtEnd() && Peek() == '.') {
            ++pos_;
            while (!AtEnd() && std::isdigit(static_cast<unsigned char>(Peek()))) {
                ++pos_;
            }
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
        while (!AtEnd() && std::isspace(static_cast<unsigned char>(Peek()))) {
            ++pos_;
        }
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

int RequireInt(const JsonValue& value, const std::string& label) {
    if (value.type != JsonValue::Type::Number) {
        throw std::runtime_error("Expected integer field: " + label);
    }
    return std::stoi(value.string_value);
}

std::uint64_t RequireUInt64(const JsonValue& value, const std::string& label) {
    if (value.type != JsonValue::Type::Number) {
        throw std::runtime_error("Expected integer field: " + label);
    }
    return static_cast<std::uint64_t>(std::stoull(value.string_value));
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

    config.problem_id = RequireString(RequireObjectField(root, "problem_id"), "problem_id");
    config.time_limit_ms = RequireInt(RequireObjectField(root, "time_limit_ms"), "time_limit_ms");
    config.max_output_bytes = RequireUInt64(RequireObjectField(root, "max_output_bytes"), "max_output_bytes");
    config.compare_mode = ParseCompareMode(RequireString(RequireObjectField(root, "compare_mode"), "compare_mode"));

    const JsonValue& paths = RequireObjectField(root, "paths");
    config.program_dir = ResolvePath(base_dir, RequireString(RequireObjectField(paths, "program_dir"), "paths.program_dir"));
    config.test_dir = ResolvePath(base_dir, RequireString(RequireObjectField(paths, "test_dir"), "paths.test_dir"));
    try {
        config.answer_dir = ResolvePath(base_dir, RequireString(RequireObjectField(paths, "answer_dir"), "paths.answer_dir"));
    } catch (const std::exception&) {
        config.answer_dir = config.test_dir;
    }
    config.report_dir = ResolvePath(base_dir, RequireString(RequireObjectField(paths, "report_dir"), "paths.report_dir"));

    if (!PathExists(config.program_dir)) {
        throw std::runtime_error("Program directory does not exist: " + config.program_dir.string());
    }
    if (!PathExists(config.test_dir)) {
        throw std::runtime_error("Test directory does not exist: " + config.test_dir.string());
    }
    if (!PathExists(config.answer_dir)) {
        throw std::runtime_error("Answer directory does not exist: " + config.answer_dir.string());
    }

    const auto& programs = RequireArray(RequireObjectField(root, "programs"), "programs");
    if (programs.empty()) {
        throw std::runtime_error("Program list must not be empty");
    }
    for (const auto& item : programs) {
        ProgramEntry entry;
        entry.id = RequireString(RequireObjectField(item, "id"), "programs[].id");
        entry.exe = RequireString(RequireObjectField(item, "exe"), "programs[].exe");
        entry.exe_path = ResolvePath(config.program_dir, entry.exe);
        if (!PathExists(entry.exe_path)) {
            throw std::runtime_error("Program executable does not exist: " + entry.exe_path.string());
        }
        config.programs.push_back(std::move(entry));
    }

    const auto& tests = RequireArray(RequireObjectField(root, "tests"), "tests");
    if (tests.empty()) {
        throw std::runtime_error("Test list must not be empty");
    }
    for (const auto& item : tests) {
        TestCase test;
        test.id = RequireString(RequireObjectField(item, "id"), "tests[].id");
        test.input = RequireString(RequireObjectField(item, "input"), "tests[].input");
        test.answer = RequireString(RequireObjectField(item, "answer"), "tests[].answer");
        test.score = RequireInt(RequireObjectField(item, "score"), "tests[].score");
        test.input_path = ResolvePath(config.test_dir, test.input);
        test.answer_path = ResolvePath(config.answer_dir, test.answer);
        if (!PathExists(test.input_path)) {
            throw std::runtime_error("Test input does not exist: " + test.input_path.string());
        }
        if (!PathExists(test.answer_path)) {
            throw std::runtime_error("Test answer does not exist: " + test.answer_path.string());
        }
        config.tests.push_back(std::move(test));
    }

    std::filesystem::create_directories(config.report_dir);
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
