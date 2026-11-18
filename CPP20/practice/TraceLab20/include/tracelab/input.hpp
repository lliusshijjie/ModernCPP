#pragma once
#include "generator.hpp"
#include "model.hpp"
#include <charconv>
#include <concepts>
#include <stdexcept>
#include <stop_token>
#include <string_view>

namespace tracelab {
struct BadRecord : std::invalid_argument {
    using std::invalid_argument::invalid_argument;
};

template <std::unsigned_integral T>
T number(std::string_view s) {
    if (s.empty()) {
        throw BadRecord("empty number");
    }
    T value{};
    auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc{} || end != s.data() + s.size()) {
        throw BadRecord("invalid unsigned integer");
    }
    return value;
}

bool valid_utf8(std::string_view);
bool identifier(std::string_view);
bool plain_text(std::string_view);
std::string bytes(std::u8string_view);
Time timestamp(std::string_view);
std::chrono::minutes offset(std::string_view);
Level parse_level(std::string_view);
Event parse(const Record&);
Generator<Record> records(fs::path path, std::size_t file, std::stop_token stop);
std::vector<fs::path> discover(const std::vector<fs::path>& inputs, std::string_view extension);
} // namespace tracelab
