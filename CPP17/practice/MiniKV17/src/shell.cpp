#include "minikv/shell.hpp"

#include <tuple>

namespace minikv {

void Shell::observe(std::any context, Observer observer) {
    context_ = std::move(context);
    observer_ = std::move(observer);
}

void Shell::help(std::ostream& output) {
    line(output, "put KEY string|int|bool VALUE [TTL_SECONDS]");
    line(output, "get KEY | erase KEY | rename OLD NEW | scan [PREFIX]");
    line(output, "incr KEY DELTA (atomic int64 addition; preserves TTL)");
    line(output, "stats | compact | help | quit");
    line(output, "Quote strings with spaces; escapes: \\\\, \\\", \\n, \\r, \\t, \\xHH. # starts a "
                 "comment line.");
}

std::size_t Shell::remember_error(std::string message) {
    failed_ = true;
    last_error_ = std::move(message);
    return ++error_count_;
}

bool Shell::print(Status status) {
    if (auto* ok = std::get_if<Ok>(&status)) {
        line(output_, "OK ", ok->message);
        return true;
    }

    auto error = std::get<Error>(std::move(status));

    // [28] << 链从左到右定序：remember_error 更新状态后，才读取 last_error_。
    output_ << "ERR " << remember_error(std::move(error.message)) << ": " << last_error_ << '\n';

    return false;
}

bool Shell::execute(Command command) {
    return std::visit(
        [&](auto&& request) -> bool {
            using T = std::decay_t<decltype(request)>;

            if constexpr (std::is_same_v<T, Put>) {
                // [15][29] 元组拥有参数；apply 通过 INVOKE 规则调用成员函数。
                auto args = std::tuple{std::ref(store_), std::move(request.key),
                                       std::move(request.value), request.ttl};
                return print(std::apply(&Store::put, std::move(args)));
            }
            else if constexpr (std::is_same_v<T, Get>) {
                if (auto entry = store_.get(request.key); entry) {
                    line(output_, "VALUE ", display(entry->value));
                } else {
                    line(output_, "MISSING");
                }
            }
            else if constexpr (std::is_same_v<T, Increment>) {
                return print(
                    std::invoke(&Store::increment, store_, std::move(request.key), request.delta));
            }
            else if constexpr (std::is_same_v<T, Erase>) {
                return print(std::invoke(&Store::erase, store_, request.key));
            }
            else if constexpr (std::is_same_v<T, Rename>) {
                return print(std::invoke(&Store::rename, store_, request.from, request.to));
            }
            else if constexpr (std::is_same_v<T, Scan>) {
                const auto rows = store_.scan(request.prefix);

                for (const auto& [key, entry] : rows) {
                    output_ << key << ' ' << display(entry.value);
                    if (entry.expires) {
                        output_ << " expires=" << *entry.expires;
                    }
                    output_ << '\n';
                }

                line(output_, "ROWS ", rows.size());
            }
            else if constexpr (std::is_same_v<T, ShowStats>) {
                const auto s = store_.stats();
                line(output_, "STATS keys=", s.keys, " bytes=", s.value_bytes, " writes=", s.writes,
                     " reads=", s.reads, " hits=", s.hits, " replayed=", s.replayed,
                     " compactions=", s.compactions, " discarded_tail=", s.discarded_tail,
                     " faulted=", s.faulted);
            }
            else if constexpr (std::is_same_v<T, Compact>) {
                return print(store_.compact());
            }
            else if constexpr (std::is_same_v<T, Help>) {
                help(output_);
            }
            else {
                line(output_, "BYE");
            }

            return true;
        },
        std::move(command));
}

int Shell::run(std::istream& input) {
    for (std::string text; std::getline(input, text);) {
        const auto first = text.find_first_not_of(" \t\r");
        if (first == std::string::npos || text[first] == '#') {
            continue;
        }

        auto parsed = parse_command(text);
        bool success = false;
        bool stop = false;
        if (auto* error = std::get_if<Error>(&parsed)) {
            print(*error);
        } else {
            auto command = std::get<Command>(std::move(parsed));
            stop = std::holds_alternative<Quit>(command);
            success = execute(std::move(command));
        }

        // 宿主观察器在 Store 调用返回后执行，不持有数据锁；抛异常不会撤销已提交命令。
        if (observer_) {
            try {
                std::invoke(observer_, context_, CommandEvent{success});
            } catch (const std::exception& error) {
                print(Error{std::string("observer: ") + error.what()});
            }
        }

        if (stop) {
            break;
        }
    }

    if (input.bad()) {
        return 2;
    }

    return failed_ ? 1 : 0;
}

} // namespace minikv
