#include "tracelab/analysis.hpp"
#include "tracelab/queue.hpp"
#include <atomic>
#include <mutex>
#include <format>
#include <source_location>
#include <thread>

namespace tracelab {
namespace {
using Batch = std::vector<Record>;
constinit std::atomic<Count> next_session{1}; // [40]

// A complete result must cover every nonempty row submitted by the reader.
void verify_coverage(Count submitted, Count processed,
                     std::source_location site = std::source_location::current()) {
    if (submitted != processed) [[unlikely]] {
        throw std::logic_error(std::format("{}:{}: submitted {}, committed {}", site.file_name(),
                                           site.line(), submitted, processed));
    }
}

class ProgressBus {
public:
    void publish(Count read, Count processed, Count matched, bool done = false) {
        {
            std::lock_guard lock(mutex_);
            value_.read += read;
            value_.processed += processed;
            value_.matched += matched;
            value_.done = done;
            version_.fetch_add(1);
        }
        version_.notify_all();
    }

    void observe(const Observer& observer) {
        for (;;) {
            // [45] Read version first. A publication between the snapshot and wait
            // changes the value, even when notification precedes the wait.
            auto version = version_.load();
            Progress current;
            {
                std::lock_guard lock(mutex_);
                current = value_;
            }
            observer(current);
            if (current.done) {
                return;
            }
            version_.wait(version);
        }
    }

private:
    Progress value_{.session = next_session.fetch_add(1)};
    std::mutex mutex_;
    std::atomic<Count> version_{};
};

template <class Submit>
void produce(const std::vector<fs::path>& files, std::stop_token stop, Submit submit) {
    Batch batch;
    std::size_t bytes_used{};
    for (std::size_t i = 0; i < files.size() && !stop.stop_requested(); ++i) {
        for (Record& row : records(files[i], i, stop)) {
            if (batch.size() == 256 || bytes_used + row.bytes.size() > 1024 * 1024) {
                if (!submit(std::move(batch))) {
                    return;
                }
                batch.clear();
                bytes_used = 0;
            }
            bytes_used += row.bytes.size();
            batch.push_back(std::move(row));
        }
    }
    if (!stop.stop_requested() && !batch.empty()) {
        submit(std::move(batch));
    }
}

Report start(std::vector<fs::path> files, const Options& options) {
    options.query.validate();
    if (files.size() > 10000 || options.workers < 1 || options.workers > 8) {
        throw std::invalid_argument("file or worker limit exceeded");
    }
    return {.status = Status::complete,
            .query = options.query.describe(),
            .files = std::move(files),
            .summary = {}};
}

void complete(Report& report, const Query& query, bool cancelled) {
    finish(report.summary, query);
    if (cancelled) {
        report.status = Status::cancelled;
    } else if (report.summary.stats.invalid) {
        report.status = Status::warnings;
    }
}
} // namespace

Report analyze_serial(std::vector<fs::path> files, const Options& options) {
    auto report = start(std::move(files), options);
    produce(report.files, {}, [&](Batch batch) {
        merge(report.summary, analyze_batch(batch, options, {}, report.files), options.query);
        return true;
    });
    complete(report, options.query, false);
    return report;
}

Report analyze(std::vector<fs::path> files, const Options options, std::stop_token external,
               Observer observer) {
    auto report = start(std::move(files), options);
    std::stop_source source;
    auto stop = source.get_token();
    std::stop_callback cancel(external, [&] { source.request_stop(); });
    BoundedQueue<Batch> queue(2 * options.workers);
    ProgressBus progress;
    std::vector<Summary> local(options.workers);
    std::mutex error_mutex;
    std::exception_ptr error;
    auto fail = [&] {
        {
            std::lock_guard lock(error_mutex);
            if (!error) {
                error = std::current_exception();
            }
        }
        source.request_stop();
    };

    // [38] The calling thread produces. Workers own separate accumulators.
    std::jthread monitor;
    std::vector<std::jthread> workers;
    Count submitted{};
    try {
        if (observer) {
            monitor = std::jthread([&] {
                try {
                    progress.observe(observer);
                } catch (...) {
                    fail();
                }
            });
        }
        for (auto& summary : local) {
            workers.emplace_back([&, slot = &summary] {
                try {
                    while (auto batch = queue.pop(stop)) {
                        auto part = analyze_batch(*batch, options, stop, report.files);
                        if (stop.stop_requested()) {
                            break;
                        }
                        merge(*slot, part, options.query);
                        if (observer) {
                            progress.publish(0, part.stats.processed, part.stats.matched);
                        }
                    }
                } catch (...) {
                    fail();
                }
            });
        }
        produce(report.files, stop, [&](Batch batch) {
            add(submitted, batch.size());
            if (observer) {
                progress.publish(batch.size(), 0, 0);
            }
            return queue.push(std::move(batch), stop);
        });
    } catch (...) {
        fail();
    }

    queue.close();
    for (auto& worker : workers) {
        worker.join();
    }
    try {
        for (const auto& summary : local) {
            merge(report.summary, summary, options.query);
        }
        complete(report, options.query, stop.stop_requested());
        if (report.status != Status::cancelled) {
            verify_coverage(submitted, report.summary.stats.processed);
        }
    } catch (...) {
        fail();
    }
    if (monitor.joinable()) {
        progress.publish(0, 0, 0, true);
        monitor.join();
    }
    if (error) {
        std::rethrow_exception(error);
    }
    return report;
}
} // namespace tracelab
