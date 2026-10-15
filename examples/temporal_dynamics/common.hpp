#pragma once
#include "rfc/numeric/exact_matrix.hpp"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace temporal_demo {
using Q = rfc::numeric::Rational;
using V = rfc::numeric::RationalVector;
using M = rfc::numeric::ExactMatrix;
inline constexpr std::uint64_t database_namespace = 97101, field_namespace = 97102;
inline std::size_t checks{};
inline void require(bool value, const char *message) {
    ++checks;
    if (!value)
        throw std::logic_error(message);
}
inline std::uint64_t integer(std::string_view value) {
    std::uint64_t out{};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), out);
    if (error != std::errc{} || end != value.data() + value.size() || value.empty())
        throw std::invalid_argument("invalid unsigned integer");
    return out;
}
inline Q rational(const std::string &text) {
    if (text.empty() || text.size() > 256)
        throw std::invalid_argument("rational spelling limit");
    Q q;
    if (q.set_str(text, 10) != 0 || q.get_den() == 0)
        throw std::invalid_argument("invalid rational");
    q.canonicalize();
    return q;
}
inline std::vector<std::string> split(const std::string &text, char separator) {
    std::vector<std::string> out;
    std::istringstream input(text);
    std::string part;
    while (std::getline(input, part, separator))
        out.push_back(part);
    return out;
}
inline std::string quote(std::string_view text) {
    constexpr char hex[] = "0123456789abcdef";
    std::string out = "\"";
    for (const char raw : text) {
        const auto c = static_cast<unsigned char>(raw);
        if (c == '"' || c == '\\') {
            out += '\\';
            out += raw;
        } else if (c < 32) {
            out += "\\u00";
            out += hex[c >> 4];
            out += hex[c & 15];
        } else
            out += raw;
    }
    return out + '"';
}
inline std::string array(const std::vector<std::string> &items) {
    std::string out = "[";
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i)
            out += ',';
        out += items[i];
    }
    return out + ']';
}
inline std::string vector(const V &values) {
    std::vector<std::string> items;
    for (const auto &q : values)
        items.push_back(quote(q.get_str()));
    return array(items);
}
inline std::string matrix(const M &value) {
    std::vector<std::string> rows;
    for (std::size_t i = 0; i < value.rows(); ++i) {
        V row(value.cols());
        for (std::size_t j = 0; j < value.cols(); ++j)
            row[j] = value.get(i, j);
        rows.push_back(vector(row));
    }
    return array(rows);
}
inline std::string numbers(const std::vector<std::uint64_t> &values) {
    std::vector<std::string> items;
    for (auto n : values)
        items.push_back(std::to_string(n));
    return array(items);
}
inline std::string strings(const std::vector<std::string> &values) {
    std::vector<std::string> items;
    for (const auto &s : values)
        items.push_back(quote(s));
    return array(items);
}
inline std::string object(std::initializer_list<std::pair<std::string, std::string>> fields) {
    std::string out = "{";
    bool first = true;
    for (const auto &[key, value] : fields) {
        if (!first)
            out += ',';
        first = false;
        out += quote(key) + ':' + value;
    }
    return out + '}';
}
inline void emit(std::initializer_list<std::pair<std::string, std::string>> fields) {
    std::cout << object(fields) << '\n';
}
inline std::string boolean(bool value) {
    return value ? "true" : "false";
}
inline void progress(const std::string &text) {
    std::cerr << text << '\n';
}
struct Event {
    std::uint64_t publication{}, available_minute{}, category{}, subtype{}, native_key{};
    std::string name;
    Q start, end, forcing;
    std::optional<Q> reading;
    std::vector<std::string> assets;
};
struct Frame {
    std::uint64_t sequence{}, available_minute{};
    std::vector<Event> events;
    std::vector<std::pair<Q, std::uint64_t>> time_subjects;
};
std::vector<Event> read_events(const std::filesystem::path &file);
std::vector<Frame> database_demo(const std::vector<Event> &events, const std::filesystem::path &output);
void query_file(const std::filesystem::path &file, std::uint64_t cutoff, const Q &start, const Q &end);
void field_demo(const std::vector<Frame> &frames, const std::filesystem::path &output);
void algebra_demo(const std::vector<Frame> &frames);
void product_demo(const std::filesystem::path &output);
} // namespace temporal_demo
