/*
    __ _____ _____ _____
 __|  |   __|     |   | |  JSON for Modern C++
|  |  |__   |  |  | | | |  version 3.11.2
|_____|_____|_____|_|___|  https://github.com/nlohmann/json

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
*/

// 这是 nlohmann/json 的单头文件版本（简化版占位）
// 实际使用时请从 https://github.com/nlohmann/json/releases 下载完整版本
// 或使用 vcpkg/conan 等包管理器安装

#ifndef NLOHMANN_JSON_HPP
#define NLOHMANN_JSON_HPP

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <memory>
#include <optional>
#include <variant>
#include <initializer_list>
#include <type_traits>

namespace nlohmann
{

    /**
     * @brief 简化版 JSON 类（用于项目演示）
     *
     * 注意：这是一个简化实现，仅用于项目框架演示
     * 生产环境请使用完整的 nlohmann/json 库
     */
    class json
    {
    public:
        // JSON 值类型枚举
        enum class value_t
        {
            null,
            boolean,
            number_integer,
            number_float,
            string,
            array,
            object
        };

    private:
        value_t m_type = value_t::null;

        // 使用联合体存储不同类型的值
        bool m_bool = false;
        int64_t m_int = 0;
        double m_float = 0.0;
        std::string m_string;
        std::vector<json> m_array;
        std::map<std::string, json> m_object;

    public:
        // 默认构造函数
        json() : m_type(value_t::null) {}

        // 从布尔值构造
        json(bool val) : m_type(value_t::boolean), m_bool(val) {}

        // 从整数构造
        json(int val) : m_type(value_t::number_integer), m_int(val) {}
        json(int64_t val) : m_type(value_t::number_integer), m_int(val) {}

        // 从浮点数构造
        json(double val) : m_type(value_t::number_float), m_float(val) {}

        // 从字符串构造
        json(const std::string &val) : m_type(value_t::string), m_string(val) {}
        json(const char *val) : m_type(value_t::string), m_string(val ? val : "") {}

        // 从初始化列表构造对象
        json(std::initializer_list<std::pair<const std::string, json>> init)
            : m_type(value_t::object)
        {
            for (const auto &item : init)
            {
                m_object[item.first] = item.second;
            }
        }

        // 类型检查方法
        bool is_null() const { return m_type == value_t::null; }
        bool is_boolean() const { return m_type == value_t::boolean; }
        bool is_number() const { return m_type == value_t::number_integer || m_type == value_t::number_float; }
        bool is_number_integer() const { return m_type == value_t::number_integer; }
        bool is_number_float() const { return m_type == value_t::number_float; }
        bool is_string() const { return m_type == value_t::string; }
        bool is_array() const { return m_type == value_t::array; }
        bool is_object() const { return m_type == value_t::object; }

        value_t type() const { return m_type; }

        // 值获取方法
        template <typename T>
        T get() const
        {
            if constexpr (std::is_same_v<T, bool>)
            {
                if (m_type != value_t::boolean)
                    throw std::runtime_error("类型不匹配: 期望布尔值");
                return m_bool;
            }
            else if constexpr (std::is_integral_v<T>)
            {
                if (m_type != value_t::number_integer)
                    throw std::runtime_error("类型不匹配: 期望整数");
                return static_cast<T>(m_int);
            }
            else if constexpr (std::is_floating_point_v<T>)
            {
                if (m_type == value_t::number_float)
                    return static_cast<T>(m_float);
                if (m_type == value_t::number_integer)
                    return static_cast<T>(m_int);
                throw std::runtime_error("类型不匹配: 期望浮点数");
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                if (m_type != value_t::string)
                    throw std::runtime_error("类型不匹配: 期望字符串");
                return m_string;
            }
            throw std::runtime_error("不支持的类型转换");
        }

        // 带默认值的获取方法
        template <typename T>
        T value(const std::string &key, const T &default_value) const
        {
            if (m_type != value_t::object)
                return default_value;
            auto it = m_object.find(key);
            if (it == m_object.end())
                return default_value;
            try
            {
                return it->second.get<T>();
            }
            catch (...)
            {
                return default_value;
            }
        }

        // 数组操作
        void push_back(const json &val)
        {
            if (m_type == value_t::null)
                m_type = value_t::array;
            if (m_type != value_t::array)
                throw std::runtime_error("不是数组类型");
            m_array.push_back(val);
        }

        size_t size() const
        {
            if (m_type == value_t::array)
                return m_array.size();
            if (m_type == value_t::object)
                return m_object.size();
            return 0;
        }

        bool empty() const
        {
            if (m_type == value_t::array)
                return m_array.empty();
            if (m_type == value_t::object)
                return m_object.empty();
            if (m_type == value_t::string)
                return m_string.empty();
            return m_type == value_t::null;
        }

        // 对象/数组访问
        json &operator[](const std::string &key)
        {
            if (m_type == value_t::null)
                m_type = value_t::object;
            if (m_type != value_t::object)
                throw std::runtime_error("不是对象类型");
            return m_object[key];
        }

        const json &operator[](const std::string &key) const
        {
            if (m_type != value_t::object)
                throw std::runtime_error("不是对象类型");
            auto it = m_object.find(key);
            if (it == m_object.end())
            {
                static json null_json;
                return null_json;
            }
            return it->second;
        }

        json &operator[](size_t index)
        {
            if (m_type != value_t::array)
                throw std::runtime_error("不是数组类型");
            return m_array[index];
        }

        const json &operator[](size_t index) const
        {
            if (m_type != value_t::array)
                throw std::runtime_error("不是数组类型");
            return m_array[index];
        }

        // 键存在性检查
        bool contains(const std::string &key) const
        {
            if (m_type != value_t::object)
                return false;
            return m_object.find(key) != m_object.end();
        }

        // 迭代器支持（简化版）
        auto begin() { return m_type == value_t::array ? m_array.begin() : m_array.end(); }
        auto end() { return m_array.end(); }
        auto begin() const { return m_type == value_t::array ? m_array.begin() : m_array.end(); }
        auto end() const { return m_array.end(); }

        // 对象迭代
        auto items() const
        {
            return m_object;
        }

        // 从文件解析（简化版 - 需要完整实现）
        static json parse(const std::string &str)
        {
            // 简化的JSON解析器 - 生产环境请使用完整版本
            json result;
            if (str.empty() || str[0] != '{')
            {
                return result;
            }

            result.m_type = value_t::object;
            // 注意：这只是占位实现，完整解析需要更复杂的逻辑
            // 实际项目中请使用完整的 nlohmann/json 库

            return result;
        }

        static json parse(std::istream &is)
        {
            std::stringstream buffer;
            buffer << is.rdbuf();
            return parse(buffer.str());
        }

        // 序列化为字符串
        std::string dump(int indent = -1) const
        {
            std::string result;
            dump_impl(result, indent, 0);
            return result;
        }

    private:
        void dump_impl(std::string &out, int indent, int current_indent) const
        {
            std::string ind(current_indent, ' ');
            std::string ind_next(current_indent + (indent > 0 ? indent : 0), ' ');
            std::string newline = indent > 0 ? "\n" : "";

            switch (m_type)
            {
            case value_t::null:
                out += "null";
                break;
            case value_t::boolean:
                out += m_bool ? "true" : "false";
                break;
            case value_t::number_integer:
                out += std::to_string(m_int);
                break;
            case value_t::number_float:
                out += std::to_string(m_float);
                break;
            case value_t::string:
                out += "\"" + escape_string(m_string) + "\"";
                break;
            case value_t::array:
                out += "[" + newline;
                for (size_t i = 0; i < m_array.size(); ++i)
                {
                    if (indent > 0)
                        out += ind_next;
                    m_array[i].dump_impl(out, indent, current_indent + indent);
                    if (i < m_array.size() - 1)
                        out += ",";
                    out += newline;
                }
                if (indent > 0)
                    out += ind;
                out += "]";
                break;
            case value_t::object:
                out += "{" + newline;
                {
                    size_t i = 0;
                    for (const auto &[key, val] : m_object)
                    {
                        if (indent > 0)
                            out += ind_next;
                        out += "\"" + key + "\": ";
                        val.dump_impl(out, indent, current_indent + indent);
                        if (i < m_object.size() - 1)
                            out += ",";
                        out += newline;
                        ++i;
                    }
                }
                if (indent > 0)
                    out += ind;
                out += "}";
                break;
            }
        }

        static std::string escape_string(const std::string &s)
        {
            std::string result;
            for (char c : s)
            {
                switch (c)
                {
                case '"':
                    result += "\\\"";
                    break;
                case '\\':
                    result += "\\\\";
                    break;
                case '\n':
                    result += "\\n";
                    break;
                case '\r':
                    result += "\\r";
                    break;
                case '\t':
                    result += "\\t";
                    break;
                default:
                    result += c;
                    break;
                }
            }
            return result;
        }

    public:
        // 静态工厂方法
        static json array()
        {
            json j;
            j.m_type = value_t::array;
            return j;
        }

        static json object()
        {
            json j;
            j.m_type = value_t::object;
            return j;
        }
    };

    // 流输出支持
    inline std::ostream &operator<<(std::ostream &os, const json &j)
    {
        os << j.dump();
        return os;
    }

} // namespace nlohmann

#endif // NLOHMANN_JSON_HPP
