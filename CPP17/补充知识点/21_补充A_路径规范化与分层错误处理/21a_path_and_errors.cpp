// 补充 21A：只读取给定的练习目录，不创建、改写或删除其中的内容。
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

void check(const std::error_code& error, const char* operation) {
    if (error) throw std::system_error(error, operation);
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: run.exe <fixture-directory>\n";
        return 2;
    }
    try {
        std::cout << std::boolalpha;
        const fs::path root{argv[1]}; // 练习目录内应有 notes.txt 和子目录 sub。
        const fs::path relative{"sub/../notes.txt"};
        std::cout << "===== 1. PRINCIPLES =====\n";
        std::cout << "lexical: " << relative.lexically_normal().generic_string()
                  << '\n'; // notes.txt；这一步不访问磁盘。

        std::error_code error;
        const auto resolved = fs::canonical(root / relative, error);
        check(error, "canonical"); // 每次调用后立即检查，避免被下一次覆盖。
        std::cout << "canonical filename: " << resolved.filename().generic_string()
                  << '\n'; // notes.txt
        const bool same = fs::equivalent(root / "notes.txt", root / relative, error);
        check(error, "equivalent");
        require(same, "paths should identify the same file");
        std::cout << "same file: " << same << '\n';

        std::cout << "===== 2. SYNTAX =====\n";
        const auto missing = root / "missing.txt";
        const auto status = fs::status(missing, error);
        // 不存在是本例的预期分支；其他错误（如权限不足）仍须报告。
        if (error && error != std::errc::no_such_file_or_directory) {
            check(error, "status");
        }
        require(status.type() == fs::file_type::not_found, "missing.txt must be absent");
        std::cout << "missing: " << !fs::exists(status) << '\n'; // true

        const auto bytes = fs::file_size(missing, error);
        require(static_cast<bool>(error), "file_size should report an error");
        require(bytes == std::numeric_limits<std::uintmax_t>::max(),
                "unexpected file_size error result");
        std::cout << "size failed: " << static_cast<bool>(error) << '\n'; // true

        std::cout << "===== 3. APPLICATION =====\n";
        std::vector<std::string> names;
        fs::directory_iterator current{root, error};
        check(error, "open directory");
        const fs::directory_iterator end;
        while (current != end) {
            const bool regular = current->is_regular_file(error);
            check(error, "inspect entry"); // 查询条目也可能失败。
            if (regular) names.push_back(current->path().filename().generic_string());
            current.increment(error);     // 不用会走异常重载的 ++current。
            check(error, "advance iterator");
        }
        std::sort(names.begin(), names.end()); // 文件系统不保证遍历顺序。
        for (const auto& name : names) std::cout << "file: " << name << '\n';
        require(names == std::vector<std::string>{"notes.txt"}, "unexpected fixture");
        std::cout << "checks: PASS\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
