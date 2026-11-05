// 第 19 课示例：综合 —— optional / variant / any 三者取舍（C++17）
//
// 结构：① 原理验证 —— 大小与开销阶梯；any 的类型擦除与 RTTI 比对
//       ② 语法演示 —— 同一操作的三种写法对照
//       ③ 应用实战 —— 配置系统：三者各司其职的标准合影
//
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra 19_optional_variant_any.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <any>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <typeinfo>
#include <variant>

// ② 复用第 17 课的 overloaded 惯用法
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// ③ 配置系统：三层不确定性，三件套各司其职
struct ServerConfig {
    std::optional<int> port;                    // 可选字段：没给就用默认
    std::variant<bool, std::string> debug;      // 封闭集合：开关或日志级别
    std::map<std::string, std::any> extras;     // 开放槽位：插件自由存放
};

// ③ 渲染 debug 字段：visit 穷尽性检查把关（新增候选类型时编译器点名）
std::string render_debug(const std::variant<bool, std::string>& d) {
    return std::visit(overloaded{
        [](bool b) { return b ? std::string("on") : std::string("off"); },
        [](const std::string& lvl) { return "level=" + lvl; }
    }, d);
}

// ③ 安全取回 any：指针风格 any_cast，不抛异常
template <class T>
T get_as(const std::map<std::string, std::any>& bag,
         const std::string& key, T def) {
    auto it = bag.find(key);
    if (it == bag.end()) return def;
    if (auto p = std::any_cast<T>(&it->second)) return *p;
    return def;                                 // 类型不匹配：给默认
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：开销阶梯 —— 栈上标记 → 栈上带下标的 union → 擦除 + 管理器
    std::cout << sizeof(std::optional<int>) << ' '           // 8  (int+bool+padding)
              << sizeof(std::variant<int, std::string>) << ' ' // 40 (string 32 + 下标 8)
              << sizeof(std::any) << '\n';                   // 16 (管理器指针 + 小缓冲)

    // 验证 2：any 的类型信息在运行期 —— typeid 比对
    std::any a = 42;
    std::cout << (a.type() == typeid(int)) << ' '
              << (a.type() == typeid(long)) << '\n';         // true false

    // 验证 3：any_cast 精确匹配 —— 存 int 取 long 直接失败
    try {
        (void)std::any_cast<long>(a);
        std::cout << "cast ok\n";                            // 走不到
    } catch (const std::bad_any_cast&) {
        std::cout << "bad_any_cast\n";                       // bad_any_cast
    }

    // 验证 4：optional/variant 的"类型在编译期"—— 拿错类型编译不过（只列不写）：
    // std::get<std::string>(std::variant<int, double>{});   // error: 非候选

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 同一个"装一个值"，三种创建 + 判有 + 判型
    std::optional<int> o = 7;
    std::variant<int, std::string> v = std::string("x");
    std::any y = 3.14;

    std::cout << o.has_value() << ' '
              << std::holds_alternative<std::string>(v) << ' '
              << (y.type() == typeid(double)) << '\n';       // true true true

    // 2.2 安全取值三兄弟：value_or / get_if / any_cast&
    std::optional<int> eo;
    std::cout << eo.value_or(-1) << ' ';                     // -1
    if (auto p = std::get_if<int>(&v)) {
        std::cout << *p << ' ';                              // 走不到（v 是 string）
    } else {
        std::cout << "not-int ";                             // not-int
    }
    if (auto p = std::any_cast<double>(&y)) {
        std::cout << *p << '\n';                             // 3.14
    }

    // 2.3 重置语义对照：optional/any 回到"空"，variant 无空可回（回第一个备选）
    o.reset();
    y.reset();
    v = 0;                                    // 切换备选，不是"清空"
    std::cout << o.has_value() << ' ' << y.has_value() << ' '
              << v.index() << '\n';                           // false false 0

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 组装配置：可选端口（没给）、封闭 debug 集合、开放插件槽
    ServerConfig cfg;
    cfg.debug = std::string("info");
    cfg.extras["retries"] = 3;                 // 插件塞 int
    cfg.extras["label"] = std::string("prod"); // 插件塞 string

    // 3.2 消费：value_or 兜底 / visit 渲染 / any_cast 安全取回
    int port = cfg.port.value_or(8080);
    std::cout << "port=" << port << '\n';                     // port=8080
    std::cout << "debug=" << render_debug(cfg.debug) << '\n'; // debug=level=info
    std::cout << "retries=" << get_as<int>(cfg.extras, "retries", 1) << ' '
              << "label=" << get_as<std::string>(cfg.extras, "label", "?")
              << ' ' << "timeout=" << get_as<int>(cfg.extras, "timeout", 30)
              << '\n';   // retries=3 label=prod timeout=30（键不存在 → 默认）

    // 3.3 切换 debug 为 bool 再渲染：同一 visit，零改动
    cfg.debug = true;
    std::cout << "debug=" << render_debug(cfg.debug) << '\n'; // debug=on

    return 0;
}
