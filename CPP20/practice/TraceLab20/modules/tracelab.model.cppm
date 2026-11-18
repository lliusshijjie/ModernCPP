module;
#include "tracelab/entry.hpp"
#include "tracelab/model.hpp"
export module tracelab.model;

// [47] Re-export existing global-module entities. Both executables link the same library.
export namespace trace_app {
using tracelab::cli;
using tracelab::Event;
using tracelab::exit_code;
using tracelab::Level;
using tracelab::Query;
using tracelab::Report;
using tracelab::Status;
} // namespace trace_app
