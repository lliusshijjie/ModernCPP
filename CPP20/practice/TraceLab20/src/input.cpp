#include "tracelab/input.hpp"
#include "tracelab/schema.hpp"
#include <algorithm>
#include <format>
#include <fstream>
#include <ranges>

namespace tracelab {
using namespace std::chrono;

std::string bytes(std::u8string_view text) {
    // [46] Copy UTF-8 code units through their object representation.
    if (text.empty()) {
        return {};
    }
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

bool valid_utf8(std::string_view text) {
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i++]);
        if (lead < 0x80) {
            continue;
        }
        unsigned tail, code, minimum;
        if (lead >= 0xc2 && lead <= 0xdf) {
            tail = 1;
            code = lead & 31;
            minimum = 0x80;
        } else if (lead >= 0xe0 && lead <= 0xef) {
            tail = 2;
            code = lead & 15;
            minimum = 0x800;
        } else if (lead >= 0xf0 && lead <= 0xf4) {
            tail = 3;
            code = lead & 7;
            minimum = 0x10000;
        } else {
            return false;
        }
        if (text.size() - i < tail) {
            return false;
        }
        while (tail--) {
            auto c = static_cast<unsigned char>(text[i++]);
            if ((c & 0xc0) != 0x80) {
                return false;
            }
            code = (code << 6) | (c & 63);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) {
            return false;
        }
    }
    return true;
}

bool plain_text(std::string_view text) {
    return text.find_first_of(std::string_view("\t\r\n\0", 4)) == text.npos && valid_utf8(text);
}

bool identifier(std::string_view text) {
    return !text.empty() && text.size() <= 128 && std::ranges::all_of(text, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '_' || c == '.' || c == '-';
    });
}

minutes offset(std::string_view s) {
    if (s == "Z") {
        return minutes{};
    }
    if (s.size() != 6 || (s[0] != '+' && s[0] != '-') || s[3] != ':') {
        throw BadRecord("expected Z or +/-HH:MM");
    }
    auto h = number<unsigned>(s.substr(1, 2));
    auto m = number<unsigned>(s.substr(4, 2));
    if (h > 23 || m > 59) {
        throw BadRecord("invalid UTC offset");
    }
    return minutes(static_cast<int>(60 * h + m) * (s[0] == '-' ? -1 : 1));
}

Time timestamp(std::string_view s) {
    // [48] Validate the civil date before converting to the UTC timeline.
    if (s.size() < 20 || s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' ||
        s[16] != ':') {
        throw BadRecord("expected YYYY-MM-DDTHH:MM:SS with offset");
    }
    auto digits = [&](unsigned at, unsigned count) {
        return number<unsigned>(s.substr(at, count));
    };
    auto date = year(static_cast<int>(digits(0, 4))) / month(digits(5, 2)) / day(digits(8, 2));
    auto h = digits(11, 2), m = digits(14, 2), sec = digits(17, 2);
    if (!date.ok() || h > 23 || m > 59 || sec > 59) {
        throw BadRecord("invalid calendar date or clock");
    }
    s.remove_prefix(19);
    unsigned us{};
    if (s.starts_with('.')) {
        s.remove_prefix(1);
        auto n = s.find_first_not_of("0123456789");
        if (n == 0 || n > 6) {
            throw BadRecord("expected 1..6 fractional digits and offset");
        }
        us = number<unsigned>(s.substr(0, n));
        s.remove_prefix(n);
        while (n++ < 6) {
            us *= 10;
        }
    }
    return sys_days(date) + hours(h) + minutes(m) + seconds(sec) + microseconds(us) - offset(s);
}

Level parse_level(std::string_view s) {
    auto it = std::ranges::find(names, s);
    if (it == names.end()) {
        throw BadRecord("unknown level");
    }
    return static_cast<Level>(it - names.begin());
}

Event parse(const Record& record) {
    std::array<std::string_view, fields.size()> parts;
    std::string_view rest = record.bytes;
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
        auto end = rest.find('\t');
        if (end == rest.npos) {
            throw BadRecord(std::format("missing field after {}", fields[i].data()));
        }
        parts[i] = rest.substr(0, end);
        rest.remove_prefix(end + 1);
    }
    parts.back() = rest;
    // [41] Keep the field name when reporting a conversion failure.
    auto convert = [&]<class F>(std::size_t i, F decode) {
        try {
            return decode(parts[i]);
        } catch (const BadRecord& e) {
            throw BadRecord(std::format("{}: {}", fields[i].data(), e.what()));
        }
    };
    Event event;
    event.key = {.time = convert(0, timestamp), .source = record.source};
    event.level = convert(1, parse_level);
    if (!identifier(parts[2])) {
        throw BadRecord("invalid component");
    }
    event.component = parts[2];
    event.pid = convert(3, number<std::uint32_t>);
    event.flags = convert(4, number<unsigned>);
    if (event.flags & ~flag_mask) {
        throw BadRecord("reserved flag bit");
    }
    if (parts[5] != "-") {
        event.duration = convert(5, number<Count>);
    }
    if (!plain_text(rest)) {
        throw BadRecord("invalid UTF-8 or delimiter in message");
    }
    event.message = rest;
    return event;
}

Generator<Record> records(fs::path path, std::size_t file, std::stop_token stop) {
    std::ifstream input(path, std::ios::binary);
    const auto label = bytes(path.generic_u8string());
    if (!input) {
        throw std::runtime_error("cannot open " + label);
    }
    Record row{{file, 1}, {}};
    while (!stop.stop_requested()) {
        char c{};
        bool got = static_cast<bool>(input.get(c));
        if (got && c != '\n') [[likely]] { // [41] Most input bytes belong to the current line.
            if (row.bytes.size() >= max_line + 4) {
                throw std::runtime_error(
                    std::format("{}:{}: line exceeds 64 KiB", label, row.source.line));
            }
            row.bytes.push_back(c);
            continue;
        }
        if (!got && (input.bad() || !input.eof())) {
            throw std::runtime_error("read failed: " + label);
        }
        if (got && row.bytes.ends_with('\r')) {
            row.bytes.pop_back();
        }
        if (row.source.line == 1 && row.bytes.starts_with("\xef\xbb\xbf")) {
            row.bytes.erase(0, 3);
        }
        if (row.bytes.size() > max_line) {
            throw std::runtime_error(
                std::format("{}:{}: line exceeds 64 KiB", label, row.source.line));
        }
        if (!row.bytes.empty()) {
            co_yield row;
        }
        if (!got) {
            break;
        }
        ++row.source.line;
        row.bytes.clear();
    }
}

std::vector<fs::path> discover(const std::vector<fs::path>& inputs, std::string_view extension) {
    if (inputs.empty()) {
        throw std::invalid_argument("provide --input");
    }
    std::set<fs::path> paths;
    auto insert = [&](const fs::path& path) {
        if (!fs::is_regular_file(path)) {
            throw std::runtime_error("expected regular file: " + bytes(path.generic_u8string()));
        }
        paths.insert(fs::canonical(path));
        if (paths.size() > 10000) {
            throw std::runtime_error("file limit exceeded");
        }
    };
    for (const auto& path : inputs) {
        if (fs::is_directory(path)) {
            for (const auto& item : fs::directory_iterator(path)) {
                if (item.is_regular_file() &&
                    bytes(item.path().extension().u8string()) == extension) {
                    insert(item.path());
                }
            }
        } else {
            insert(path);
        }
    }
    return {paths.begin(), paths.end()};
}
} // namespace tracelab
