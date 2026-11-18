#pragma once

namespace tracelab {
enum class Status { complete, warnings, failed, cancelled };

constexpr int exit_code(Status status) noexcept {
    return static_cast<int>(status);
}
} // namespace tracelab
