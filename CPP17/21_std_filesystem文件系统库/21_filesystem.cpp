// 第 21 课示例：std::filesystem 文件系统库（C++17）
//
// 结构：① 原理验证 —— path 是纯词法；分解与拼接；错误处理双轨制
//       ② 语法演示 —— 规范化；绝对/相对；类型查询
//       ③ 应用实战 —— 建目录树 -> 遍历统计 -> 清场（全程确定输出）
//
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra 21_filesystem.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：path 是纯词法操作 —— 不存在的路径照样分解
    fs::path p = "dir/hello.txt";
    std::cout << p.parent_path().generic_string() << ' '
              << p.filename().generic_string() << ' '
              << p.stem().generic_string() << ' '
              << p.extension().generic_string() << '\n';
    // dir hello.txt hello .txt

    // 验证 2：operator/ 拼接自动补分隔符；无分隔符连接用 +=（没有 operator+！）
    fs::path joined = fs::path("dir") / "sub" / "f.txt";
    std::cout << joined.generic_string() << '\n';    // dir/sub/f.txt
    fs::path cat = fs::path("dir");
    cat += "x";                                      // concat：无分隔符连接
    std::cout << cat.generic_string() << '\n';       // dirx

    // 验证 3：错误处理双轨制 —— 同一操作的两个重载
    try {
        (void)fs::file_size("definitely_missing.txt");    // 抛异常版
        std::cout << "no throw\n";                        // 走不到
    } catch (const fs::filesystem_error&) {
        std::cout << "filesystem_error\n";                // filesystem_error
    }
    std::error_code ec;
    (void)fs::file_size("definitely_missing.txt", ec);    // 错误码版：不抛
    std::cout << static_cast<bool>(ec) << '\n';           // true

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 词法规范化：不查盘，纯字符串归一
    std::cout << fs::path("a/b/../c").lexically_normal().generic_string() << ' '
              << fs::path("./a//b/./c.txt").lexically_normal().generic_string()
              << '\n';                                    // a/c a/b/c.txt

    // 2.2 绝对/相对判断
    std::cout << fs::path("fs_demo_tmp").is_absolute() << ' '
              << fs::current_path().is_absolute() << '\n'; // false true

    // 2.3 查盘类：不存在的路径（exists 不算错误，返回 false 即可）
    std::cout << fs::exists("fs_demo_tmp") << '\n';       // false（还没建）

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 搭建临时目录树：create_directories 幂等（已存在不报错）
    fs::create_directories("fs_demo_tmp/sub");
    std::ofstream("fs_demo_tmp/a.txt") << "hi";           // 2 字节
    std::ofstream("fs_demo_tmp/sub/b.cpp") << "int main(){}";  // 12 字节
    std::cout << fs::exists("fs_demo_tmp") << '\n';       // true

    // 3.2 非递归遍历：收集再排序（遍历顺序未指定，排序保证可复现）
    std::vector<std::string> names;
    for (const auto& e : fs::directory_iterator("fs_demo_tmp")) {
        names.push_back(e.path().generic_string());
    }
    std::sort(names.begin(), names.end());
    for (const auto& n : names) std::cout << n << ' ';
    std::cout << '\n';    // fs_demo_tmp/a.txt fs_demo_tmp/sub

    // 3.3 递归遍历统计：directory_entry 缓存属性，避免重复系统调用
    int files = 0, cpp_files = 0;
    std::uintmax_t total = 0;
    std::vector<std::string> listing;
    for (const auto& e : fs::recursive_directory_iterator("fs_demo_tmp")) {
        if (e.is_regular_file()) {
            ++files;
            if (e.path().extension() == ".cpp") ++cpp_files;
            total += e.file_size();
            listing.push_back(e.path().generic_string() + " (" +
                              std::to_string(e.file_size()) + ")");
        }
    }
    std::sort(listing.begin(), listing.end());
    for (const auto& row : listing) std::cout << row << '\n';
    // fs_demo_tmp/a.txt (2)
    // fs_demo_tmp/sub/b.cpp (12)
    std::cout << "files=" << files << " cpp=" << cpp_files
              << " bytes=" << total << '\n';               // files=2 cpp=1 bytes=14

    // 3.4 清场：remove_all 返回删除的条目数（文件和目录都计数，含根自身）
    std::cout << fs::remove_all("fs_demo_tmp") << ' '
              << fs::exists("fs_demo_tmp") << '\n';        // 4 false

    return 0;
}
