#include "minikv/shell.hpp"

#include <iostream>

int main(int argc, char** argv) {
    using namespace minikv;

    try {
        fs::path directory = "minikv17-data";
        std::optional<fs::path> script;

        for (int i = 1; i < argc; ++i) {
            const std::string_view arg(argv[i]);
            if (arg == "--help") {
                line(std::cout, kVersion, " [--dir DIRECTORY] [--script FILE]");
                Shell::help(std::cout);
                return 0;
            }

            if ((arg != "--dir" && arg != "--script") || i + 1 == argc) {
                throw std::invalid_argument("use --dir DIRECTORY, --script FILE or --help");
            }
            if (arg == "--dir") {
                directory = argv[++i];
            } else {
                script = argv[++i];
            }
        }

        // 先检查脚本，再创建数据库目录；参数错误不产生存储副作用。
        std::ifstream input;
        if (script) {
            input.open(*script);
            if (!input) {
                throw std::runtime_error("cannot open script");
            }
        }

        auto store = Store::open(directory); // [27] 即使禁用可选 NRVO 也可成立
        Shell shell(store, std::cout);
        shell.observe(SessionAudit{}, [](std::any& context, const CommandEvent& event) {
            // [18] 指针版 any_cast 检查实际类型，不依赖异常进行类型探测。
            if (auto* audit = std::any_cast<SessionAudit>(&context)) {
                ++audit->commands;
                if (!event.success) {
                    ++audit->errors;
                }
            }
        });

        if (!script) {
            line(std::cout, kVersion, " ready. Type help; Ctrl+Z/Ctrl+D ends input.");
        }

        const auto code = shell.run(script ? input : std::cin);
        if (auto* audit = std::any_cast<SessionAudit>(&shell.context())) {
            line(std::cout, "SESSION commands=", audit->commands, " errors=", audit->errors);
        }

        return code;
    } catch (const std::exception& error) {
        line(std::cerr, "FATAL ", error.what());
        return 2;
    }
}
