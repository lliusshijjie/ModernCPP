#include "assetforge/report.hpp"
#include "detail.hpp"
#include <algorithm>
#include <charconv>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <sstream>

namespace assetforge {
namespace {
constexpr std::string_view help = R"(AssetForge17
  assetforge plan MANIFEST (--profile NAME | --root ID)
  assetforge build MANIFEST (--profile NAME | --root ID) --out FILE [--policy seq|par] [--report NAME]
  assetforge inspect FILE [--id NAME] [--detail]
  assetforge verify FILE [--policy seq|par]
  assetforge shell MANIFEST --profile NAME
  Reports: normalization, sizes
)";

struct Usage : Error {
    using Error::Error;
};

using Arguments = std::vector<std::string>;

class Options {
public:
    Options(const Arguments& args, std::size_t start,
            std::initializer_list<std::string_view> valued,
            std::initializer_list<std::string_view> flags = {}) {
        for (auto i = start; i < args.size(); ++i) {
            const auto& key = args[i];
            if (values_.find(key) != values_.end()) {
                throw Usage("duplicate option: " + key);
            }

            if (std::find(flags.begin(), flags.end(), key) != flags.end()) {
                values_.emplace(key, "");
            } else if (std::find(valued.begin(), valued.end(), key) != valued.end()) {
                if (++i >= args.size() || args[i].empty() || args[i].substr(0, 2) == "--") {
                    throw Usage("missing value for " + key);
                }
                values_.emplace(key, args[i]);
            } else {
                throw Usage("unknown option: " + key);
            }
        }
    }

    std::optional<std::string_view> get(std::string_view key) const {
        if (auto it = values_.find(key); it != values_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::string_view required(std::string_view key) const {
        if (auto value = get(key)) {
            return *value;
        }
        throw Usage("required option: " + std::string(key));
    }

private:
    std::map<std::string, std::string, std::less<>> values_;
};

fs::path path(std::string_view text) {
    if (text.empty() || text.find('\0') != text.npos) {
        throw Usage("expected nonempty file path");
    }
    return fs::u8path(text);
}

Policy policy(const Options& options) {
    auto name = options.get("--policy").value_or("seq");
    if (name == "seq") {
        return Policy::seq;
    }
    if (name == "par") {
        return Policy::par;
    }
    throw Usage("expected --policy seq or par");
}

Selection selection(const Options& options) {
    auto profile = options.get("--profile");
    auto root = options.get("--root");
    if (profile.has_value() == root.has_value()) {
        throw Usage("choose exactly one of --profile and --root");
    }
    if (profile) {
        return Profile{std::string(*profile)};
    }
    return Root{std::string(*root)};
}

struct Output {
    fs::path file;
    Policy policy;
    std::optional<std::string> report;
};

Output output(const Options& options) {
    std::optional<std::string> report;
    if (auto name = options.get("--report")) {
        report.emplace(*name);
    }
    return {path(options.required("--out")), policy(options), std::move(report)};
}

struct PlanCommand {
    fs::path manifest;
    Selection selection;
};

struct BuildCommand {
    fs::path manifest;
    Selection selection;
    Output output;
};

struct InspectCommand {
    fs::path file;
    std::optional<std::string> id;
    bool detail;
};

struct VerifyCommand {
    fs::path file;
    Policy policy;
};

struct ShellCommand {
    fs::path manifest;
    Profile profile;
};

using Command =
    std::variant<PlanCommand, BuildCommand, InspectCommand, VerifyCommand, ShellCommand>;

Command parse(const Arguments& args) {
    if (args.size() < 2 || args[1].substr(0, 2) == "--") {
        throw Usage("expected input path");
    }
    const auto file = path(args[1]);
    if (args[0] == "plan") {
        const Options options(args, 2, {"--profile", "--root"});
        return PlanCommand{file, selection(options)};
    }
    if (args[0] == "build") {
        const Options options(args, 2, {"--profile", "--root", "--out", "--policy", "--report"});
        return BuildCommand{file, selection(options), output(options)};
    }
    if (args[0] == "inspect") {
        const Options options(args, 2, {"--id"}, {"--detail"});
        std::optional<std::string> id;
        if (auto name = options.get("--id")) {
            id.emplace(*name);
        }
        return InspectCommand{file, std::move(id), options.get("--detail").has_value()};
    }
    if (args[0] == "verify") {
        return VerifyCommand{file, policy(Options(args, 2, {"--policy"}))};
    }
    if (args[0] == "shell") {
        const Options options(args, 2, {"--profile"});
        return ShellCommand{file, Profile{std::string(options.required("--profile"))}};
    }
    throw Usage("unknown command: " + args[0]);
}

void print_plan(const BuildPlan& plan) {
    for (std::size_t i = 0; i < plan.assets().size(); ++i) {
        const auto& entry = plan.assets()[i];
        std::cout << "PLAN " << decimal(i) << ' ' << entry.spec.id;
        for (auto dep : entry.dependencies) {
            std::cout << " requires=" << plan.assets()[dep].spec.id;
        }
        std::cout << '\n';
    }
    for (const auto& [name, index] : plan.aliases()) {
        std::cout << "ALIAS " << name << '=' << plan.assets()[index].spec.id << '\n';
    }
}

void do_build(const BuildPlan& plan, const Output& output) {
    auto reports = default_reports();
    if (output.report && !reports.contains(*output.report)) {
        throw Usage("unknown report: " + *output.report);
    }

    auto result = build(plan, output.file, output.policy);
    std::visit(detail::Overloaded{[&](const BuildSummary& summary) {
                                      std::cout
                                          << "BUILT assets=" << decimal(summary.metrics.size())
                                          << " aliases=" << decimal(summary.alias_count)
                                          << " bytes=" << decimal(summary.content_size) << '\n';
                                      if (output.report) {
                                          try {
                                              std::cout << reports.render(*output.report, summary)
                                                        << '\n';
                                          } catch (const std::exception& error) {
                                              std::cerr << "report: " << error.what() << '\n';
                                          }
                                      }
                                  },
                                  [](const BuildError& error) { throw Error(error.message); }},
               result);
}

std::string hex32(std::uint32_t value) {
    std::array<char, 8> text;
    auto [end, status] = std::to_chars(text.data(), text.data() + text.size(), value, 16);
    if (status != std::errc{}) {
        throw std::logic_error("CRC buffer is too small");
    }
    return std::string(8 - (end - text.data()), '0') + std::string(text.data(), end);
}

void print_entry(const BundleReader& reader, Index index, bool detail) {
    const auto& entry = reader.entries()[index];
    enum class View { detail, summary };
    switch (detail ? View::detail : View::summary) {
    case View::detail:
        for (auto dep : entry.dependencies) {
            std::cout << "REQUIRES " << entry.id << ' ' << reader.entries()[dep].id << '\n';
        }
        for (const auto& [name, target] : reader.aliases()) {
            if (target == index) {
                std::cout << "ALIAS " << name << '=' << entry.id << '\n';
            }
        }
        [[fallthrough]]; // [22] Detail includes the same summary row.
    case View::summary:
        std::cout << "ENTRY " << entry.id << " offset=" << decimal(entry.offset)
                  << " size=" << decimal(entry.size) << " crc=" << hex32(entry.crc) << '\n';
    }
}

int inspect(const InspectCommand& command) {
    auto reader = BundleReader::open(command.file);
    if (command.id) {
        auto id = reader.find(*command.id);
        if (!id) {
            throw Error("unknown asset or alias: " + *command.id);
        }
        print_entry(reader, *id, command.detail);
    } else {
        std::cout << "BUNDLE assets=" << decimal(reader.entries().size())
                  << " aliases=" << decimal(reader.aliases().size())
                  << " bytes=" << decimal(reader.content_size()) << '\n';
        for (Index i = 0; i < reader.entries().size(); ++i) {
            print_entry(reader, i, command.detail);
        }
    }
    return 0;
}

int verify(const VerifyCommand& command) {
    auto reader = BundleReader::open(command.file);
    auto failures = reader.verify(command.policy);
    for (const auto& id : failures) {
        std::cerr << "BAD CRC " << id << '\n';
    }
    if (!failures.empty()) {
        return 1;
    }
    std::cout << "VERIFIED assets=" << decimal(reader.entries().size())
              << " bytes=" << decimal(reader.content_size()) << '\n';
    return 0;
}

Arguments words(const std::string& text) {
    std::istringstream input(text);
    Arguments result;
    while (input >> std::ws && input.peek() != std::char_traits<char>::eof()) {
        std::string word;
        if (!(input >> std::quoted(word))) {
            throw Usage("unclosed quoted argument");
        }
        result.push_back(std::move(word));
    }
    return result;
}

int shell(const ShellCommand& command) {
    auto manifest = fs::absolute(command.manifest);
    Workspace workspace(load_catalog(manifest), command.profile);
    std::string line;
    int status{};
    while (std::getline(std::cin, line)) {
        try {
            auto args = words(line);
            if (args.empty()) {
                continue;
            }
            const auto& action = args.front();
            if (action == "quit" && args.size() == 1) {
                break;
            }

            if (action == "help" && args.size() == 1) {
                std::cout << "plan | build --out FILE [--policy seq|par] [--report NAME]\n"
                             "use PROFILE | reload [MANIFEST] | rename-alias OLD NEW | quit\n";
            } else if (action == "use" && args.size() == 2) {
                workspace.select(Profile{args[1]});
                std::cout << "USING " << workspace.selected().name << '\n';
            } else if (action == "reload" && args.size() <= 2) {
                auto next = args.size() == 2 ? fs::absolute(path(args[1])) : manifest;
                workspace.reload(next);
                manifest.swap(next);
                std::cout << "RELOADED\n";
            } else if (action == "rename-alias" && args.size() == 3) {
                workspace.rename_alias(args[1], args[2]);
                std::cout << "RENAMED " << args[1] << " -> " << args[2] << '\n';
            } else if ((action == "plan" && args.size() == 1) || action == "build") {
                auto snapshot = workspace.snapshot();
                auto build_plan = plan(snapshot.catalog, snapshot.profile);
                if (action == "plan") {
                    print_plan(build_plan);
                } else {
                    do_build(build_plan,
                             output(Options(args, 1, {"--out", "--policy", "--report"})));
                }
            } else {
                throw Usage("unknown shell command or arguments; use help");
            }
        } catch (const std::exception& error) {
            status = 1;
            std::cerr << "assetforge: " << error.what() << '\n';
        }
    }
    if (std::cin.bad()) {
        throw Error("cannot read shell commands");
    }
    return status;
}

int run(const Arguments& args) {
    if (args.empty() || (args.size() == 1 && (args[0] == "--help" || args[0] == "help"))) {
        std::cout << help;
        return 0;
    }
    return std::visit(detail::Overloaded{
                          [](const PlanCommand& cmd) {
                              print_plan(plan(load_catalog(cmd.manifest), cmd.selection));
                              return 0;
                          },
                          [](const BuildCommand& cmd) {
                              do_build(plan(load_catalog(cmd.manifest), cmd.selection), cmd.output);
                              return 0;
                          },
                          [](const InspectCommand& cmd) { return inspect(cmd); },
                          [](const VerifyCommand& cmd) { return verify(cmd); },
                          [](const ShellCommand& cmd) { return shell(cmd); }},
                      parse(args));
}
} // namespace
} // namespace assetforge

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    using namespace assetforge;
    try {
        auto arguments = Arguments{};
        for (int i = 1; i < argc; ++i) {
#ifdef _WIN32
            arguments.push_back(fs::path(argv[i]).u8string());
#else
            arguments.emplace_back(argv[i]);
#endif
        }
        return run(arguments);
    } catch (const Usage& error) {
        std::cerr << "assetforge: " << error.what() << '\n' << help;
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "assetforge: " << error.what() << '\n';
        return 1;
    }
}
