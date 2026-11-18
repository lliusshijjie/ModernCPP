#pragma once
#include "status.hpp"

namespace tracelab {
#ifdef _WIN32
Status cli(int argc, wchar_t** argv);
#else
Status cli(int argc, char** argv);
#endif
} // namespace tracelab
