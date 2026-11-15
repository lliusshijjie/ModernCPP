#pragma once

#include "store.hpp"

#include <istream>

namespace minikv {

struct CommandEvent {
    bool success;
};

struct SessionAudit {
    std::size_t commands = 0;
    std::size_t errors = 0;
};

class Shell {
public:
    using Observer = std::function<void(std::any&, const CommandEvent&)>;

    Shell(Store& store, std::ostream& output) : store_(store), output_(output) {
    }

    // [18] 宿主自选扩展状态类型，核心存储不需要知道该类型；状态不写入 WAL。
    void observe(std::any context, Observer observer);

    const std::any& context() const {
        return context_;
    }

    int run(std::istream& input);

    static void help(std::ostream& output);

private:
    Store& store_;
    std::ostream& output_;

    std::any context_;
    Observer observer_;

    bool failed_ = false;
    std::size_t error_count_ = 0;
    std::string last_error_;

    bool execute(Command command);

    bool print(Status status);

    std::size_t remember_error(std::string message);
};

} // namespace minikv
