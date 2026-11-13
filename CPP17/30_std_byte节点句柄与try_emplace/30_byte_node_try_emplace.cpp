// 第 30 课示例：std::byte、map 节点句柄与 try_emplace（C++17）—— CPP17 收官课
//
// 结构：① 原理验证 —— std::byte 只认位运算；try_emplace 的参数保全
//       ② 语法演示 —— insert_or_assign；节点拔出/改名/插入/冲突路径
//       ③ 应用实战 —— merge 整表合并（冲突留源表）
//
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra 30_byte_node_try_emplace.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <cstddef>    // std::byte
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：std::byte —— 位运算畅通，算术被拒，转整数必须显式
    std::byte b{0x0F};
    b |= std::byte{0x10};              // 0x1F
    b <<= 1;                           // 0x3E
    std::cout << std::to_integer<int>(b) << '\n';          // 62
    // b += 1;   // 编译错：std::byte 不参与算术 —— 这正是它的存在意义
    // b = 5;    // 编译错：无隐式整数转换（必须 std::byte{5}）

    // 验证 2：emplace 键冲突 —— 参数已被消耗（unique_ptr 保证变空）
    std::map<std::string, std::unique_ptr<int>> m;
    m["dup"] = std::make_unique<int>(1);
    auto p = std::make_unique<int>(42);
    auto [it1, ok1] = m.emplace("dup", std::move(p));
    std::cout << ok1 << ' ' << (p == nullptr) << '\n';     // false true —— p 被搬空！

    // 验证 3：try_emplace 键冲突 —— 参数分毫未动
    auto q = std::make_unique<int>(7);
    auto [it2, ok2] = m.try_emplace("dup", std::move(q));
    std::cout << ok2 << ' ' << (q != nullptr) << ' ' << *q << '\n';
    // false true 7 —— 冲突时 move 根本没发生

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 insert_or_assign：存在覆盖（无默认构造要求），不存在插入
    auto [it3, ins3] = m.insert_or_assign("dup", std::make_unique<int>(9));
    std::cout << ins3 << ' ' << *m["dup"] << '\n';          // false 9（覆盖）
    auto [it4, ins4] = m.insert_or_assign("fresh", std::make_unique<int>(4));
    std::cout << ins4 << ' ' << *m["fresh"] << '\n';        // true 4（新插入）

    // 2.2 节点句柄：拔出 → 改 key（map 独有）→ 插入另一张表
    std::map<std::string, int> a{{"old", 1}, {"keep", 2}};
    std::map<std::string, int> b2{{"conflict", 0}};
    auto h = a.extract("old");                              // 拔节点：零拷贝
    std::cout << a.size() << ' ' << !h.empty() << '\n';     // 1 true
    h.key() = "renamed";                                    // 改 key 的唯一正道
    auto ins = b2.insert(std::move(h));
    std::cout << ins.inserted << ' ' << b2.at("renamed") << ' '
              << h.empty() << '\n';                         // true 1 true

    // 2.3 冲突插入：insert(node_type&&) 返回 insert_return_type{inserted, position, node}
    //     失败时节点从"返回值的 .node"回到你手上（h2 已被 move-from 变空）——
    //     不是"留在 h2 里"！想收回节点就接住返回值
    auto h2 = a.extract("keep");
    h2.key() = "conflict";                                  // 制造键冲突
    auto r2 = b2.insert(std::move(h2));                     // insert_return_type
    std::cout << r2.inserted << ' ' << !r2.node.empty() << ' '
              << r2.node.key() << '\n';                     // false true conflict
    // r2.node 持有节点，离开作用域析构即销毁 —— 安全

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 merge 整表合并：不冲突的搬走，冲突的留在源表
    //     b2 现有 {conflict:0, renamed:1}；h2 析构销毁
    std::map<std::string, int> src{{"x", 1}, {"conflict", 5}};
    b2.merge(src);
    std::cout << b2.size() << ' ' << src.size() << '\n';    // 3 1（conflict 留在 src）
    std::cout << b2.at("x") << '\n';                        // 1

    // 3.2 实战心法（对照 ①②③）：
    //   值可移动 + 可能冲突 -> try_emplace（保参数）
    //   总要写入           -> insert_or_assign（免默认构造）
    //   搬家/改名/合并     -> extract / key() / merge（零拷贝）
    return 0;
}
