#pragma once

#include "model.hpp"

#include <filesystem>
#include <fstream>

namespace minikv {

namespace fs = std::filesystem;

struct Recovery {
    std::vector<Mutation> records;
    std::uint64_t discarded = 0;
};

class Journal {
public:
    explicit Journal(fs::path directory);

    Journal(const Journal&) = delete;
    Journal& operator=(const Journal&) = delete;

    Recovery recover();

    [[nodiscard]] Status append(const Mutation& mutation);

    [[nodiscard]] Status rewrite(const std::vector<Row>& rows);

    [[nodiscard]] bool faulted() const {
        return faulted_;
    }

private:
    fs::path active_;
    fs::path backup_;
    fs::path next_;

    std::ofstream output_;
    bool faulted_ = false;

    void open_append();
};

} // namespace minikv
