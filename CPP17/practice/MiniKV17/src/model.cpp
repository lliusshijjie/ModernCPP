#include "minikv/model.hpp"

#include <chrono>

namespace minikv {

bool valid_key(std::string_view key) {
    if (key.empty() || key.size() > kMaxKey) {
        return false;
    }

    for (unsigned char c : key) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-' || c == '.' || c == '/')) {
            return false;
        }
    }

    return true;
}

bool expired(const Entry& entry, std::int64_t now) {
    return entry.expires && *entry.expires <= now;
}

std::int64_t wall_seconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::uint64_t value_size(const Value& value) {
    return std::visit(
        [](const auto& item) -> std::uint64_t {
            if constexpr (std::is_same_v<std::decay_t<decltype(item)>, std::string>) {
                return item.size();
            } else if constexpr (std::is_same_v<std::decay_t<decltype(item)>, bool>) {
                return 1;
            } else {
                return 8;
            }
        },
        value);
}

std::string quote(std::string_view text) {
    std::string result = "\"";
    constexpr char hex[] = "0123456789abcdef";

    for (unsigned char c : text) {
        switch (c) {
        case '\\':
            result += "\\\\";
            break;
        case '"':
            result += "\\\"";
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
            if (c < 32 || c == 127) {
                result += "\\x";
                result += hex[c >> 4];
                result += hex[c & 15];
            } else {
                result += static_cast<char>(c);
            }
        }
    }

    return result + '"';
}

std::string display(const Value& value) {
    return std::visit(
        [](const auto& item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return "string " + quote(item);
            } else if constexpr (std::is_same_v<T, bool>) {
                return "bool " + scalar_text(item);
            } else {
                return "int " + scalar_text(item);
            }
        },
        value);
}

namespace {

bool space(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

int hex_digit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    return -1;
}

// [20] 扫描期间借用输入；离开解析器的 token 和 Command 都拥有字符串。
Result<std::vector<std::string>> tokenize(std::string_view text) {
    std::vector<std::string> tokens;
    std::size_t pos = 0;

    while (pos < text.size()) {
        if (space(text[pos])) {
            ++pos;
            continue;
        }
        if (tokens.empty() && text[pos] == '#') {
            break;
        }
        if (tokens.size() == 6) {
            return Error{"too many arguments"};
        }

        std::string token;
        if (text[pos] == '"') {
            bool closed = false;
            for (++pos; pos < text.size(); ++pos) {
                char c = text[pos];
                if (c == '"') {
                    ++pos;
                    closed = true;
                    break;
                }

                if (c == '\\') {
                    if (++pos == text.size()) {
                        return Error{"unfinished escape"};
                    }
                    switch (text[pos]) {
                    case 'n':
                        c = '\n';
                        break;
                    case 'r':
                        c = '\r';
                        break;
                    case 't':
                        c = '\t';
                        break;
                    case 'x': {
                        if (text.size() - pos < 3) {
                            return Error{"short hex escape"};
                        }

                        const auto high = hex_digit(text[pos + 1]);
                        const auto low = hex_digit(text[pos + 2]);
                        if (high < 0 || low < 0) {
                            return Error{"invalid hex escape"};
                        }

                        c = static_cast<char>(high * 16 + low);
                        pos += 2;
                        break;
                    }
                    case '\\':
                        [[fallthrough]];
                    case '"':
                        c = text[pos];
                        break;
                    default:
                        return Error{"unknown escape"};
                    }
                }

                token += c;
            }

            if (!closed) {
                return Error{"unclosed quote"};
            }
            if (pos < text.size() && !space(text[pos])) {
                return Error{"expected space after quote"};
            }
        } else {
            const auto start = pos;
            while (pos < text.size() && !space(text[pos])) {
                if (text[pos] == '"') {
                    return Error{"quote must start a token"};
                }
                ++pos;
            }
            token.assign(text.substr(start, pos - start));
        }

        tokens.push_back(std::move(token));
    }

    return tokens;
}

} // namespace

Result<Command> parse_command(std::string_view text) {
    if (text.size() > kMaxValue * 4 + 1024) {
        return Error{"command too long"};
    }

    auto parsed = tokenize(text);
    if (auto* error = std::get_if<Error>(&parsed)) {
        return *error;
    }

    auto tokens = std::get<std::vector<std::string>>(std::move(parsed));
    if (tokens.empty()) {
        return Error{"empty command"};
    }

    const auto& name = tokens[0];
    const auto n = tokens.size();

    if (name == "put" && (n == 4 || n == 5)) {
        if (!valid_key(tokens[1])) {
            return Error{"invalid key"};
        }

        Value value;
        if (tokens[2] == "string") {
            value = std::move(tokens[3]);
        } else if (tokens[2] == "int") {
            // [12] 查找结果的名字只在该分支链内可见。
            if (auto integer = number<std::int64_t>(tokens[3]); integer) {
                value = *integer;
            } else {
                return Error{"invalid int64"};
            }
        } else if (tokens[2] == "bool" && (tokens[3] == "true" || tokens[3] == "false")) {
            value = tokens[3] == "true";
        } else {
            return Error{"expected string, int or bool value"};
        }

        if (value_size(value) > kMaxValue) {
            return Error{"value too large"};
        }

        std::optional<std::int64_t> ttl;
        if (n == 5) {
            ttl = number<std::int64_t>(tokens[4]);
            if (!ttl || *ttl <= 0) {
                return Error{"TTL must be positive seconds"};
            }
        }

        return Command{Put{std::move(tokens[1]), std::move(value), ttl}};
    }

    if (name == "get" && n == 2 && valid_key(tokens[1])) {
        return Command{Get{tokens[1]}};
    }

    if (name == "incr" && n == 3 && valid_key(tokens[1])) {
        if (auto delta = number<std::int64_t>(tokens[2]); delta) {
            return Command{Increment{tokens[1], *delta}};
        }
        return Error{"invalid increment"};
    }

    if (name == "erase" && n == 2 && valid_key(tokens[1])) {
        return Command{Erase{tokens[1]}};
    }

    if (name == "rename" && n == 3 && valid_key(tokens[1]) && valid_key(tokens[2])) {
        return Command{Rename{tokens[1], tokens[2]}};
    }

    if (name == "scan" && (n == 1 || (n == 2 && (tokens[1].empty() || valid_key(tokens[1]))))) {
        return Command{Scan{n == 2 ? tokens[1] : ""}};
    }

    if (name == "stats" && n == 1) {
        return Command{ShowStats{}};
    }

    if (name == "compact" && n == 1) {
        return Command{Compact{}};
    }

    if (name == "help" && n == 1) {
        return Command{Help{}};
    }

    if (name == "quit" && n == 1) {
        return Command{Quit{}};
    }

    return Error{"unknown command, wrong arguments or invalid key; use help"};
}

} // namespace minikv
